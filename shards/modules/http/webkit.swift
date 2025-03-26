import Foundation
import shards
import WebKit

class ShardsTypes {
    static let shared = ShardsTypes()

    static let FragCC: Int32 = 0x6672_6167

    let AnyType = TypeInfo(type: VarType.AnyValue)
    let AnySeqType: TypeInfo
    let AnyTableType: TypeInfo
    let AnyVarType: TypeInfo

    let ShardType = TypeInfo(type: VarType.ShardRef)
    let ShardSeqType: TypeInfo
    let ShardVarType: TypeInfo

    let NoneType = TypeInfo(type: VarType.NoValue)
    let NoneVarType: TypeInfo

    let StringType = TypeInfo(type: VarType.String)
    let StringVarType: TypeInfo

    let IntType = TypeInfo(type: VarType.Int)
    let IntVarType: TypeInfo

    let FloatType = TypeInfo(type: VarType.Float)
    let FloatVarType: TypeInfo
    let Float3Type = TypeInfo(type: VarType.Float3)
    let Float3VarType: TypeInfo
    let Float4Type = TypeInfo(type: VarType.Float4)
    let Float4VarType: TypeInfo

    let BoolType = TypeInfo(type: VarType.Bool)
    let BoolVarType: TypeInfo

    let ColorType = TypeInfo(type: VarType.Color)
    let ColorVarType: TypeInfo

    let AnyTypes: Types
    let AnySeqTypes: Types
    let AnyTableTypes: Types
    let StringTypes: Types
    let IntTypes: Types
    let NoneTypes: Types
    let FloatTypes: Types
    let Float3Types: Types
    let Float4Types: Types
    let BoolTypes: Types
    let ColorTypes: Types

    init() {
        AnySeqType = TypeInfo(seqOf: AnyType)
        AnyTableType = TypeInfo(tableOf: AnyType)
        ShardSeqType = TypeInfo(seqOf: ShardType)
        AnyTypes = Types(types: [AnyType])
        AnySeqTypes = Types(types: [AnySeqType])
        AnyTableTypes = Types(types: [AnyTableType])
        StringTypes = Types(types: [StringType])
        IntTypes = Types(types: [IntType])
        NoneTypes = Types(types: [NoneType])
        FloatTypes = Types(types: [FloatType])
        Float3Types = Types(types: [Float3Type])
        Float4Types = Types(types: [Float4Type])
        BoolTypes = Types(types: [BoolType])
        ColorTypes = Types(types: [ColorType])
        AnyVarType = TypeInfo(variableOf: AnyType)
        ShardVarType = TypeInfo(variableOf: ShardType)
        NoneVarType = TypeInfo(variableOf: NoneType)
        StringVarType = TypeInfo(variableOf: StringType)
        IntVarType = TypeInfo(variableOf: IntType)
        FloatVarType = TypeInfo(variableOf: FloatType)
        Float3VarType = TypeInfo(variableOf: Float3Type)
        Float4VarType = TypeInfo(variableOf: Float4Type)
        BoolVarType = TypeInfo(variableOf: BoolType)
        ColorVarType = TypeInfo(variableOf: ColorType)
    }
}

final class WebViewShard: IShard {
    static var name: StaticString = "WebKit.Fetch"
    static var help: StaticString = "Loads a URL in a headless WebView and extracts its text content. Input is a URL string, output is the extracted text."

    private var webView: WKWebView?
    private var outputBuffer = OwnedVar(string: "")
    var navigationComplete = false
    var extractedText: String?
    var error: Error?
    private var delegate: WebViewDelegate?

    required init() {}

    var inputTypes = ShardsTypes.shared.StringTypes
    var outputTypes = ShardsTypes.shared.StringTypes
    var parameters = Parameters()
    func setParam(idx _: Int, value _: SHVar) -> Result<Void, ShardError> {
        .failure(ShardError(message: "Not implemented"))
    }

    func getParam(idx _: Int) -> SHVar {
        SHVar()
    }

    var exposedVariables = ExposedTypes()
    var requiredVariables = ExposedTypes()

    func compose(data _: SHInstanceData) -> Result<SHTypeInfo, ShardError> {
        .success(VarType.String.asSHTypeInfo())
    }

    func warmup(context _: Context) -> Result<Void, ShardError> {
        // No initialization needed in warmup anymore
        return .success(())
    }

    func cleanup(context _: Context) -> Result<Void, ShardError> {
        webView?.navigationDelegate = nil
        webView?.stopLoading()
        webView = nil
        delegate = nil
        return .success(())
    }

    func activate(context: Context, input: SHVar) -> Result<SHVar, ShardError> {
        guard input.type == .String else {
            return .failure(ShardError(message: "Expected string input (URL)"))
        }

        guard let url = URL(string: input.string) else {
            return .failure(ShardError(message: "Invalid URL"))
        }

        // Lazy initialization of WebView if needed
        if webView == nil {
            let initGroup = DispatchGroup()
            initGroup.enter()

            var initError: Error?
            // First create a temporary WebView to get the real user agent
            let tempWebView = WKWebView(frame: .zero)
            tempWebView.evaluateJavaScript("navigator.userAgent") { result, error in
                if let error = error {
                    initError = error
                    initGroup.leave()
                    return
                }

                let configuration = WKWebViewConfiguration()

                // Use the actual device user agent if available, fallback to default if not
                if let userAgent = result as? String {
                    configuration.applicationNameForUserAgent = userAgent
                }

                // Configure website data handling
                let dataStore = WKWebsiteDataStore.default()
                configuration.websiteDataStore = dataStore
                configuration.processPool = WKProcessPool()

                // Enable JavaScript and DOM manipulation
                configuration.preferences.javaScriptEnabled = true
                if let preferences = configuration.preferences as? WKWebpagePreferences {
                    preferences.allowsContentJavaScript = true
                }

                // Create WebView with configuration
                self.webView = WKWebView(frame: .zero, configuration: configuration)
                self.delegate = WebViewDelegate(shard: self)
                self.webView?.navigationDelegate = self.delegate

                // Additional WebView settings
                if let userAgent = result as? String {
                    self.webView?.customUserAgent = userAgent
                }

                // Clean up temp WebView
                tempWebView.stopLoading()

                initGroup.leave()
            }

            // Wait for initialization with timeout
            let initTimeout = Date().addingTimeInterval(10) // Increased timeout for UA detection
            while Date() < initTimeout {
                if initGroup.wait(timeout: .now()) == .success {
                    break
                }
                if Shards.suspend(context, 0.1) != SHWireState(rawValue: 0) {
                    return .failure(ShardError(message: "Suspended during WebView initialization"))
                }
            }

            if Date() >= initTimeout {
                return .failure(ShardError(message: "WebView initialization timeout"))
            }

            if let error = initError {
                return .failure(ShardError(message: "Failed to initialize WebView: \(error.localizedDescription)"))
            }
        }

        guard let webView = webView else {
            return .failure(ShardError(message: "WebView not initialized"))
        }

        // Reset state
        navigationComplete = false
        extractedText = nil
        error = nil

        // Load URL on main thread
        // Create request with common headers
        var request = URLRequest(url: url)
        request.setValue("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", forHTTPHeaderField: "Accept")
        request.setValue("en-US,en;q=0.9", forHTTPHeaderField: "Accept-Language")
        request.setValue("no-cache", forHTTPHeaderField: "Cache-Control")

        webView.load(request)

        // Wait for navigation to complete with timeout
        let timeout = Date().addingTimeInterval(30) // 30 second timeout
        while !navigationComplete && Date() < timeout {
            if Shards.suspend(context, 0.1) != SHWireState(rawValue: 0) {
                return .failure(ShardError(message: "Suspended during page load"))
            }
        }

        if !navigationComplete {
            return .failure(ShardError(message: "Page load timeout"))
        }

        if let error = error {
            return .failure(ShardError(message: "Failed to load page: \(error.localizedDescription)"))
        }

        // Extract text using JavaScript
        let extractPromise = DispatchSemaphore(value: 0)

        let js = """
            function extractText() {
                return document.body.innerText;
            }
            extractText();
        """

        webView.evaluateJavaScript(js) { result, error in
            if let error = error {
                self.error = error
            } else if let text = result as? String {
                self.extractedText = text
            }
            extractPromise.signal()
        }

        // Wait for text extraction with timeout
        let extractTimeout = Date().addingTimeInterval(10) // 10 second timeout
        while extractedText == nil && error == nil && Date() < extractTimeout {
            if Shards.suspend(context, 0.1) != SHWireState(rawValue: 0) {
                return .failure(ShardError(message: "Suspended during text extraction"))
            }
        }

        if Date() >= extractTimeout {
            return .failure(ShardError(message: "Text extraction timeout"))
        }

        if let error = error {
            return .failure(ShardError(message: "Failed to extract text: \(error.localizedDescription)"))
        }

        guard let text = extractedText else {
            return .failure(ShardError(message: "No text extracted"))
        }

        outputBuffer.set(string: text)
        return .success(outputBuffer.v)
    }

    static func register() {
        RegisterShard(WebViewShard.name.utf8Start.withMemoryRebound(to: Int8.self, capacity: 1) { $0 }, { createSwiftShard(WebViewShard.self) })
    }

    // -- DON'T EDIT THE FOLLOWING --
    typealias ShardType = WebViewShard
    static var inputTypesCFunc: SHInputTypesProc {{ bridgeInputTypes(ShardType.self, shard: $0) }}
    static var outputTypesCFunc: SHInputTypesProc {{ bridgeOutputTypes(ShardType.self, shard: $0) }}
    static var destroyCFunc: SHDestroyProc {{ bridgeDestroy(ShardType.self, shard: $0) }}
    static var nameCFunc: SHNameProc {{ _ in bridgeName(ShardType.self) }}
    static var hashCFunc: SHHashProc {{ _ in bridgeHash(ShardType.self) }}
    static var helpCFunc: SHHelpProc {{ _ in bridgeHelp(ShardType.self) }}
    static var parametersCFunc: SHParametersProc {{ bridgeParameters(ShardType.self, shard: $0) }}
    static var setParamCFunc: SHSetParamProc {{ bridgeSetParam(ShardType.self, shard: $0, idx: $1, input: $2) }}
    static var getParamCFunc: SHGetParamProc {{ bridgeGetParam(ShardType.self, shard: $0, idx: $1) }}
    static var exposedVariablesCFunc: SHExposedVariablesProc {{ bridgeExposedVariables(ShardType.self, shard: $0) }}
    static var requiredVariablesCFunc: SHRequiredVariablesProc {{ bridgeRequiredVariables(ShardType.self, shard: $0) }}
    static var composeCFunc: SHComposeProc {{ bridgeCompose(ShardType.self, shard: $0, data: $1) }}
    static var warmupCFunc: SHWarmupProc {{ bridgeWarmup(ShardType.self, shard: $0, ctx: $1) }}
    static var cleanupCFunc: SHCleanupProc {{ bridgeCleanup(ShardType.self, shard: $0, ctx: $1) }}
    static var activateCFunc: SHActivateProc {{ bridgeActivate(ShardType.self, shard: $0, ctx: $1, input: $2) }}
    var errorCache: ContiguousArray<CChar> = []
    var output: SHVar = .init()
}

// WebView delegate to handle navigation events
private class WebViewDelegate: NSObject, WKNavigationDelegate {
    weak var shard: WebViewShard?

    init(shard: WebViewShard) {
        self.shard = shard
    }

    func webView(_: WKWebView, didFinish _: WKNavigation!) {
        shard?.navigationComplete = true
    }

    func webView(_: WKWebView, didFail _: WKNavigation!, withError error: Error) {
        shard?.error = error
        shard?.navigationComplete = true
    }

    func webView(_: WKWebView, didFailProvisionalNavigation _: WKNavigation!, withError error: Error) {
        shard?.error = error
        shard?.navigationComplete = true
    }
}

@_cdecl("shards_webkit_register")
public func shards_webkit_register() {
    WebViewShard.register()
}
