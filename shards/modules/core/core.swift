import Foundation
import shards

#if canImport(UIKit)
    import SafariServices
    import UIKit
#elseif canImport(AppKit)
    import AppKit
#endif

@_cdecl("shards_openURL")
public func openURL(_ urlString: SHStringWithLen, inApp: Bool, viewControllerPtr: UnsafeMutableRawPointer?) {
    guard let urlStr = urlString.toString(), let url = URL(string: urlStr) else { return }

    #if canImport(UIKit)
        if inApp {
            if let viewControllerPtr = viewControllerPtr {
                let viewController = Unmanaged<UIViewController>.fromOpaque(viewControllerPtr).takeUnretainedValue()
                let safariVC = SFSafariViewController(url: url)
                viewController.present(safariVC, animated: true, completion: nil)
            } else {
                UIApplication.shared.open(url, options: [:], completionHandler: nil)
            }
        } else {
            UIApplication.shared.open(url, options: [:], completionHandler: nil)
        }
    #elseif canImport(AppKit)
        NSWorkspace.shared.open(url)
    #endif
}
