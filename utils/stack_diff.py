import lldb

def stack_diff(debugger, command, result, internal_dict):
    print("Running stack_diff...")
    thread = debugger.GetSelectedTarget().GetProcess().GetSelectedThread()
    current_frame = thread.GetSelectedFrame()
    current_frame_idx = current_frame.GetFrameID()
    parent_frame = thread.GetFrameAtIndex(current_frame_idx + 1)

    if parent_frame is None or not parent_frame.IsValid():
        print("No parent frame available.")
        return

    current_sp = current_frame.GetSP()
    parent_sp = parent_frame.GetSP()
    diff = parent_sp - current_sp
    print(f"Stack Pointer Difference: {diff} bytes")

def stack_total_size(debugger, command, result, internal_dict):
    print("Running stack_total_size...")
    thread = debugger.GetSelectedTarget().GetProcess().GetSelectedThread()
    num_frames = thread.GetNumFrames()

    if num_frames == 0:
        print("No stack frames available.")
        return

    first_frame_sp = thread.GetFrameAtIndex(0).GetSP()
    last_frame_sp = thread.GetFrameAtIndex(num_frames - 1).GetSP()

    total_diff = first_frame_sp - last_frame_sp
    print(f"Total Stack Size: {total_diff} bytes")

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f stack_diff.stack_diff stack_diff')
    debugger.HandleCommand('command script add -f stack_diff.stack_total_size stack_total_size')
    print("stack_diff and stack_total_size commands have been loaded.")
