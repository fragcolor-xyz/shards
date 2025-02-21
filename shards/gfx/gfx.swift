import MetalKit

@_cdecl("gfx_metal_get_surface_size")
public func gfx_metal_get_surface_size(_ surface: UnsafeMutableRawPointer?, _ width: UnsafeMutablePointer<UInt32>?, _ height: UnsafeMutablePointer<UInt32>?) {
    if let metalLayer = surface?.assumingMemoryBound(to: CAMetalLayer.self).pointee {
        let drawableSize = metalLayer.drawableSize
        width?.pointee = UInt32(drawableSize.width)
        height?.pointee = UInt32(drawableSize.height)
    }
}
