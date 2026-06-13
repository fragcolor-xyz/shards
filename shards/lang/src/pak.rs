//! Self-extracting single-file apps: `shards pak`.
//!
//! `shards pak main.shs` produces a standalone executable that embeds a compiled
//! script (`.sho`) and carries the whole Shards runtime (the `shards` binary you
//! packed with). No interpreter, runtime files or toolchain are needed to run it.
//!
//! Unlike a naive "append to the end of the file" trick, the payload is embedded
//! in a way that keeps the container **structurally valid and signable**, so it
//! does not trip code-signing / antivirus heuristics:
//!
//! * **macOS** — the payload is placed inside the `__LINKEDIT` segment (which is
//!   grown to cover it) and the binary is then (re-)signed with `codesign`, so the
//!   signature's `codeLimit` includes the payload. `codesign` rejects data that
//!   sits *after* the Mach-O image ("strict validation"), so the payload must live
//!   inside a segment — which this does. At runtime the payload is found
//!   immediately before the code-signature blob (`LC_CODE_SIGNATURE.dataoff`).
//! * **Windows** — the payload is added as an `RT_RCDATA` resource via
//!   `UpdateResource`. The PE stays well-formed and Authenticode-signable (sign
//!   last). At runtime it is read with `FindResource`/`LoadResource` — no file I/O.
//! * **Linux** — the payload is added as a real `.shards_pak` ELF section when an
//!   `objcopy`/`llvm-objcopy` is available, otherwise appended as an overlay
//!   (ELF has no signing/Gatekeeper gate, so an overlay is harmless). At runtime
//!   it is read from the section in `/proc/self/exe`, falling back to the overlay.
//!
//! In every case a fixed 8-byte magic (`SHRDPAK1`) marks the payload so a plain,
//! un-packed `shards` binary is never mistaken for a packed one.

use crate::error::Error;

/// Magic marking a packed payload. Bump the trailing digit on format changes.
pub const PAK_MAGIC: &[u8; 8] = b"SHRDPAK1";

/// Name of the ELF section / Windows resource used to carry the payload.
#[allow(dead_code)]
const PAK_SECTION: &str = ".shards_pak";
#[allow(dead_code)]
const PAK_RESOURCE_ID: u16 = 1;

/// How the produced executable should be signed.
#[derive(Debug, Default, Clone)]
pub struct SignOpts {
  /// Do not sign at all (macOS: leaves an invalid signature — for testing only).
  pub no_sign: bool,
  /// Signing identity. macOS: `codesign -s <identity>` (default ad-hoc `-`).
  /// Windows: certificate subject/thumbprint passed to `signtool`.
  pub identity: Option<String>,
  /// macOS: run `xcrun notarytool` + staple after signing (requires creds).
  pub notarize: bool,
  /// Notarization keychain profile name (`xcrun notarytool ... --keychain-profile`).
  pub notarize_profile: Option<String>,
}

/// Embed `payload` (the compiled `.sho` bytes) into a copy of the currently
/// running `shards` executable, writing the standalone app to `out_path`.
pub fn embed_payload(payload: &[u8], out_path: &str, sign: &SignOpts) -> Result<(), Error> {
  #[cfg(target_os = "macos")]
  {
    embed_macho(payload, out_path, sign)
  }
  #[cfg(target_os = "windows")]
  {
    embed_pe(payload, out_path, sign)
  }
  #[cfg(target_os = "linux")]
  {
    embed_elf(payload, out_path, sign)
  }
  #[cfg(not(any(target_os = "macos", target_os = "windows", target_os = "linux")))]
  {
    let _ = (payload, out_path, sign);
    Err("pak: unsupported platform".into())
  }
}

/// Detect and return the payload embedded in *this* executable, if any.
/// Returns `Ok(None)` for a plain (un-packed) `shards` binary.
pub fn load_self_payload() -> Result<Option<Vec<u8>>, Error> {
  #[cfg(target_os = "macos")]
  {
    extract_macho()
  }
  #[cfg(target_os = "windows")]
  {
    extract_pe()
  }
  #[cfg(target_os = "linux")]
  {
    extract_elf()
  }
  #[cfg(not(any(target_os = "macos", target_os = "windows", target_os = "linux")))]
  {
    Ok(None)
  }
}

/// Build the 16-byte trailer `[u64 LE payload_len][8-byte magic]`.
fn make_footer(payload_len: usize) -> [u8; 16] {
  let mut footer = [0u8; 16];
  footer[0..8].copy_from_slice(&(payload_len as u64).to_le_bytes());
  footer[8..16].copy_from_slice(PAK_MAGIC);
  footer
}

/// Mark a file executable on Unix (no-op elsewhere).
#[allow(unused_variables)]
fn set_executable(path: &str) -> Result<(), Error> {
  #[cfg(unix)]
  {
    use std::os::unix::fs::PermissionsExt;
    let mut perms = std::fs::metadata(path)?.permissions();
    perms.set_mode(0o755);
    std::fs::set_permissions(path, perms)?;
  }
  Ok(())
}

// =============================================================================
// macOS — embed inside __LINKEDIT, then codesign
// =============================================================================

#[cfg(target_os = "macos")]
mod macho {
  pub const MH_MAGIC_64: u32 = 0xFEED_FACF;
  pub const FAT_MAGIC: u32 = 0xCAFE_BABE;
  pub const FAT_CIGAM: u32 = 0xBEBA_FECA;
  pub const LC_SEGMENT_64: u32 = 0x19;
  pub const LC_CODE_SIGNATURE: u32 = 0x1D;
  pub const HEADER_64_SIZE: usize = 32;
  pub const PAGE_SIZE: u64 = 0x4000; // 16 KiB pages on arm64 macOS

  #[inline]
  pub fn u32_at(b: &[u8], o: usize) -> u32 {
    u32::from_le_bytes(b[o..o + 4].try_into().unwrap())
  }
  #[inline]
  pub fn u64_at(b: &[u8], o: usize) -> u64 {
    u64::from_le_bytes(b[o..o + 8].try_into().unwrap())
  }
}

#[cfg(target_os = "macos")]
fn embed_macho(payload: &[u8], out_path: &str, sign: &SignOpts) -> Result<(), Error> {
  use macho::*;

  let exe = std::env::current_exe()?;
  std::fs::copy(&exe, out_path)?;

  // Strip the existing signature so the file ends exactly at the __LINKEDIT data.
  let _ = std::process::Command::new("codesign")
    .args(["--remove-signature", out_path])
    .output();

  let mut data = std::fs::read(out_path)?;
  if data.len() < HEADER_64_SIZE {
    return Err("pak: host executable too small".into());
  }
  let magic = u32_at(&data, 0);
  if magic == FAT_MAGIC || magic == FAT_CIGAM {
    return Err("pak: universal (fat) Mach-O is not supported; pak a thin (single-arch) `shards`".into());
  }
  if magic != MH_MAGIC_64 {
    return Err("pak: host executable is not a 64-bit Mach-O".into());
  }

  // Locate the __LINKEDIT segment load command.
  let ncmds = u32_at(&data, 16) as usize;
  let mut off = HEADER_64_SIZE;
  let mut le_off = None;
  for _ in 0..ncmds {
    if off + 8 > data.len() {
      break;
    }
    let cmd = u32_at(&data, off);
    let cmdsize = u32_at(&data, off + 4) as usize;
    if cmdsize == 0 {
      break;
    }
    if cmd == LC_SEGMENT_64 && data[off + 8..off + 24].starts_with(b"__LINKEDIT") {
      le_off = Some(off);
    }
    off += cmdsize;
  }
  let le_off = le_off.ok_or("pak: no __LINKEDIT segment found")?;
  // segment_command_64: ... vmsize@32, fileoff@40, filesize@48
  let vmsize = u64_at(&data, le_off + 32);
  let fileoff = u64_at(&data, le_off + 40);
  let filesize = u64_at(&data, le_off + 48);
  let le_end = (fileoff + filesize) as usize;
  if le_end > data.len() {
    return Err("pak: malformed __LINKEDIT (extends past EOF)".into());
  }

  // After stripping the signature the file ends at the __LINKEDIT data; drop slack.
  data.truncate(le_end);

  // Append [pad][payload][footer] so that the 16-byte footer ends on a 16-byte
  // boundary and the payload sits immediately before the footer. codesign aligns
  // the signature to 16 bytes and places it right after, so at runtime the footer
  // is exactly at `LC_CODE_SIGNATURE.dataoff - 16`.
  let footer = make_footer(payload.len());
  let unaligned_footer_start = data.len() + payload.len();
  let pad = (16 - (unaligned_footer_start % 16)) % 16;
  data.extend(std::iter::repeat(0u8).take(pad));
  data.extend_from_slice(payload);
  data.extend_from_slice(&footer);

  // Grow __LINKEDIT to cover the appended bytes.
  let added = (data.len() - le_end) as u64;
  let new_filesize = filesize + added;
  let mut new_vmsize = vmsize + added;
  if new_vmsize % PAGE_SIZE != 0 {
    new_vmsize = (new_vmsize + PAGE_SIZE - 1) & !(PAGE_SIZE - 1);
  }
  data[le_off + 32..le_off + 40].copy_from_slice(&new_vmsize.to_le_bytes());
  data[le_off + 48..le_off + 56].copy_from_slice(&new_filesize.to_le_bytes());

  std::fs::write(out_path, &data)?;
  set_executable(out_path)?;

  // (Re-)sign so the payload is inside the signed region.
  if !sign.no_sign {
    let identity = sign.identity.as_deref().unwrap_or("-");
    let out = std::process::Command::new("codesign")
      .args(["--force", "--timestamp=none", "--sign", identity, out_path])
      .output()?;
    if !out.status.success() {
      return Err(format!(
        "pak: codesign failed: {}",
        String::from_utf8_lossy(&out.stderr).trim()
      )
      .into());
    }
    if sign.notarize {
      notarize_macos(out_path, sign)?;
    }
  }
  Ok(())
}

#[cfg(target_os = "macos")]
fn notarize_macos(path: &str, sign: &SignOpts) -> Result<(), Error> {
  let profile = sign
    .notarize_profile
    .as_deref()
    .ok_or("pak: --notarize requires --notarize-profile <keychain-profile>")?;
  shards::shlog!("Submitting {} for notarization (profile {})", path, profile);
  let out = std::process::Command::new("xcrun")
    .args(["notarytool", "submit", path, "--keychain-profile", profile, "--wait"])
    .output()?;
  if !out.status.success() {
    return Err(format!(
      "pak: notarization failed: {}",
      String::from_utf8_lossy(&out.stderr).trim()
    )
    .into());
  }
  // Stapling a bare executable is not supported (only bundles/dmgs); the ticket is
  // served online. We still surface notarytool output for the user.
  shards::shlog!("{}", String::from_utf8_lossy(&out.stdout).trim());
  Ok(())
}

#[cfg(target_os = "macos")]
fn extract_macho() -> Result<Option<Vec<u8>>, Error> {
  use macho::*;
  use std::io::Read;

  let exe = std::env::current_exe()?;
  let mut f = std::fs::File::open(&exe)?;

  let mut header = [0u8; HEADER_64_SIZE];
  if f.read_exact(&mut header).is_err() {
    return Ok(None);
  }
  if u32_at(&header, 0) != MH_MAGIC_64 {
    return Ok(None); // not a thin 64-bit Mach-O; nothing we can do
  }
  let ncmds = u32_at(&header, 16) as usize;
  let sizeofcmds = u32_at(&header, 20) as usize;
  let mut cmds = vec![0u8; sizeofcmds];
  if f.read_exact(&mut cmds).is_err() {
    return Ok(None);
  }

  // Find the code signature offset (the payload footer is right before it).
  let mut off = 0usize;
  let mut sig_dataoff: Option<u64> = None;
  for _ in 0..ncmds {
    if off + 8 > cmds.len() {
      break;
    }
    let cmd = u32_at(&cmds, off);
    let cmdsize = u32_at(&cmds, off + 4) as usize;
    if cmd == LC_CODE_SIGNATURE {
      sig_dataoff = Some(u32_at(&cmds, off + 8) as u64);
    }
    if cmdsize == 0 {
      break;
    }
    off += cmdsize;
  }

  let total = f.metadata()?.len();
  // Try the footer just before the signature first, then the bare EOF (unsigned).
  for footer_end in [sig_dataoff, Some(total)].into_iter().flatten() {
    if let Some(p) = read_footer_payload(&mut f, footer_end)? {
      return Ok(Some(p));
    }
  }
  Ok(None)
}

/// Read a payload whose 16-byte footer ends at `footer_end`. Returns `None` if
/// the magic does not match.
#[cfg(any(target_os = "macos", target_os = "linux"))]
fn read_footer_payload(
  f: &mut std::fs::File,
  footer_end: u64,
) -> Result<Option<Vec<u8>>, Error> {
  use std::io::{Read, Seek, SeekFrom};
  if footer_end < 16 {
    return Ok(None);
  }
  f.seek(SeekFrom::Start(footer_end - 16))?;
  let mut footer = [0u8; 16];
  if f.read_exact(&mut footer).is_err() {
    return Ok(None);
  }
  if &footer[8..16] != PAK_MAGIC {
    return Ok(None);
  }
  let plen = u64::from_le_bytes(footer[0..8].try_into().unwrap());
  if plen == 0 || plen + 16 > footer_end {
    return Ok(None);
  }
  f.seek(SeekFrom::Start(footer_end - 16 - plen))?;
  let mut payload = vec![0u8; plen as usize];
  f.read_exact(&mut payload)?;
  Ok(Some(payload))
}

// =============================================================================
// Linux — real .shards_pak section via objcopy when available, else an overlay
// =============================================================================

#[cfg(target_os = "linux")]
fn embed_elf(payload: &[u8], out_path: &str, _sign: &SignOpts) -> Result<(), Error> {
  let exe = std::env::current_exe()?;
  std::fs::copy(&exe, out_path)?;

  // Prefer a proper, named ELF section if an objcopy is on PATH.
  if let Some(objcopy) = ["objcopy", "llvm-objcopy"]
    .into_iter()
    .find(|c| which(c).is_some())
  {
    let tmp = format!("{}.pak.bin", out_path);
    // The section payload carries its own footer so the runtime can validate it.
    let mut blob = Vec::with_capacity(payload.len() + 16);
    blob.extend_from_slice(payload);
    blob.extend_from_slice(&make_footer(payload.len()));
    std::fs::write(&tmp, &blob)?;
    // A plain --add-section creates a non-allocated read-only section, which is
    // exactly what we want: the runtime reads it by file offset from
    // /proc/self/exe, so it never needs to be mapped into the process image.
    let status = std::process::Command::new(objcopy)
      .arg("--add-section")
      .arg(format!("{}={}", PAK_SECTION, tmp))
      .arg(out_path)
      .arg(out_path)
      .status();
    let _ = std::fs::remove_file(&tmp);
    if matches!(status, Ok(s) if s.success()) {
      set_executable(out_path)?;
      return Ok(());
    }
    // fall through to overlay on objcopy failure
  }

  // Fallback: append the payload + footer as an overlay (harmless on ELF).
  let mut data = std::fs::read(out_path)?;
  let footer = make_footer(payload.len());
  data.extend_from_slice(payload);
  data.extend_from_slice(&footer);
  std::fs::write(out_path, &data)?;
  set_executable(out_path)?;
  Ok(())
}

#[cfg(target_os = "linux")]
fn extract_elf() -> Result<Option<Vec<u8>>, Error> {
  let mut f = std::fs::File::open("/proc/self/exe")?;

  // First try the named .shards_pak section.
  if let Some((sec_off, sec_size)) = elf_find_section(&mut f, PAK_SECTION)? {
    if sec_size >= 16 {
      if let Some(p) = read_footer_payload(&mut f, sec_off + sec_size)? {
        return Ok(Some(p));
      }
    }
  }

  // Fallback: overlay at EOF.
  let total = f.metadata()?.len();
  if let Some(p) = read_footer_payload(&mut f, total)? {
    return Ok(Some(p));
  }
  Ok(None)
}

/// Minimal ELF64 section lookup by name. Returns `(file_offset, size)`.
#[cfg(target_os = "linux")]
fn elf_find_section(
  f: &mut std::fs::File,
  name: &str,
) -> Result<Option<(u64, u64)>, Error> {
  use std::io::{Read, Seek, SeekFrom};

  let mut ident = [0u8; 64];
  f.seek(SeekFrom::Start(0))?;
  if f.read_exact(&mut ident).is_err() {
    return Ok(None);
  }
  if &ident[0..4] != b"\x7fELF" || ident[4] != 2 {
    return Ok(None); // not ELF64
  }
  let rd_u16 = |b: &[u8], o: usize| u16::from_le_bytes(b[o..o + 2].try_into().unwrap());
  let rd_u64 = |b: &[u8], o: usize| u64::from_le_bytes(b[o..o + 8].try_into().unwrap());
  // ELF64 header: e_shoff@40, e_shentsize@58, e_shnum@60, e_shstrndx@62
  let e_shoff = rd_u64(&ident, 40);
  let e_shentsize = rd_u16(&ident, 58) as u64;
  let e_shnum = rd_u16(&ident, 60) as u64;
  let e_shstrndx = rd_u16(&ident, 62) as u64;
  if e_shoff == 0 || e_shnum == 0 {
    return Ok(None);
  }

  // Read the section header table.
  let sh_table_size = (e_shentsize * e_shnum) as usize;
  let mut sh = vec![0u8; sh_table_size];
  f.seek(SeekFrom::Start(e_shoff))?;
  f.read_exact(&mut sh)?;

  // Locate the section header string table (.shstrtab).
  let shstr_hdr = (e_shstrndx * e_shentsize) as usize;
  // section_header: sh_offset@24, sh_size@32
  let shstr_off = rd_u64(&sh, shstr_hdr + 24);
  let shstr_size = rd_u64(&sh, shstr_hdr + 32);
  let mut shstr = vec![0u8; shstr_size as usize];
  f.seek(SeekFrom::Start(shstr_off))?;
  f.read_exact(&mut shstr)?;

  for i in 0..e_shnum as usize {
    let base = i * e_shentsize as usize;
    let name_off = u32::from_le_bytes(sh[base..base + 4].try_into().unwrap()) as usize;
    let end = shstr[name_off..].iter().position(|&c| c == 0).unwrap_or(0) + name_off;
    if &shstr[name_off..end] == name.as_bytes() {
      let off = rd_u64(&sh, base + 24);
      let size = rd_u64(&sh, base + 32);
      return Ok(Some((off, size)));
    }
  }
  Ok(None)
}

#[cfg(target_os = "linux")]
fn which(cmd: &str) -> Option<std::path::PathBuf> {
  let path = std::env::var_os("PATH")?;
  for dir in std::env::split_paths(&path) {
    let candidate = dir.join(cmd);
    if candidate.is_file() {
      return Some(candidate);
    }
  }
  None
}

// =============================================================================
// Windows — payload as an RT_RCDATA resource via UpdateResource
// =============================================================================

// RT_RCDATA == MAKEINTRESOURCE(10); resource type/name are PCWSTR (= *const u16)
// values whose integer lives in the low word.
#[cfg(target_os = "windows")]
const RT_RCDATA_W: *const u16 = 10usize as *const u16;

#[cfg(target_os = "windows")]
fn embed_pe(payload: &[u8], out_path: &str, sign: &SignOpts) -> Result<(), Error> {
  use std::os::windows::ffi::OsStrExt;
  use windows_sys::Win32::System::LibraryLoader::{
    BeginUpdateResourceW, EndUpdateResourceW, UpdateResourceW,
  };

  let exe = std::env::current_exe()?;
  std::fs::copy(&exe, out_path)?;

  let wide: Vec<u16> = std::path::Path::new(out_path)
    .as_os_str()
    .encode_wide()
    .chain(std::iter::once(0))
    .collect();

  // The resource payload carries its own footer (for symmetry / validation).
  let mut blob = Vec::with_capacity(payload.len() + 16);
  blob.extend_from_slice(payload);
  blob.extend_from_slice(&make_footer(payload.len()));

  let res_name = PAK_RESOURCE_ID as usize as *const u16;

  unsafe {
    let h = BeginUpdateResourceW(wide.as_ptr(), 0);
    if h.is_null() {
      return Err("pak: BeginUpdateResource failed".into());
    }
    let ok = UpdateResourceW(
      h,
      RT_RCDATA_W,
      res_name,
      0, // neutral language
      blob.as_ptr().cast(),
      blob.len() as u32,
    );
    if ok == 0 {
      EndUpdateResourceW(h, 1);
      return Err("pak: UpdateResource failed".into());
    }
    if EndUpdateResourceW(h, 0) == 0 {
      return Err("pak: EndUpdateResource failed".into());
    }
  }

  // Authenticode signing (optional; user supplies the certificate).
  if !sign.no_sign {
    if let Some(identity) = sign.identity.as_deref() {
      let out = std::process::Command::new("signtool")
        .args(["sign", "/fd", "SHA256", "/n", identity, out_path])
        .output()?;
      if !out.status.success() {
        return Err(format!(
          "pak: signtool failed: {}",
          String::from_utf8_lossy(&out.stderr).trim()
        )
        .into());
      }
    }
  }
  Ok(())
}

#[cfg(target_os = "windows")]
fn extract_pe() -> Result<Option<Vec<u8>>, Error> {
  use windows_sys::Win32::System::LibraryLoader::{
    FindResourceW, GetModuleHandleW, LoadResource, LockResource, SizeofResource,
  };

  let res_name = PAK_RESOURCE_ID as usize as *const u16;

  unsafe {
    let module = GetModuleHandleW(std::ptr::null());
    // FindResourceW(hModule, lpName, lpType)
    let res = FindResourceW(module, res_name, RT_RCDATA_W);
    if res.is_null() {
      return Ok(None);
    }
    let size = SizeofResource(module, res) as usize;
    let handle = LoadResource(module, res);
    if handle.is_null() || size < 16 {
      return Ok(None);
    }
    let ptr = LockResource(handle) as *const u8;
    if ptr.is_null() {
      return Ok(None);
    }
    let blob = std::slice::from_raw_parts(ptr, size);
    // Validate footer and strip it.
    let footer = &blob[size - 16..];
    if &footer[8..16] != PAK_MAGIC {
      return Ok(None);
    }
    let plen = u64::from_le_bytes(footer[0..8].try_into().unwrap()) as usize;
    if plen == 0 || plen + 16 > size {
      return Ok(None);
    }
    Ok(Some(blob[size - 16 - plen..size - 16].to_vec()))
  }
}
