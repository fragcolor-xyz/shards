// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "Shards",
    platforms: [
        .macOS(.v12),
        .iOS(.v15),
        .visionOS(.v1)
    ],
    products: [
        .library(
            name: "Shards",
            targets: ["Shards"]
        ),
    ],
    targets: [
        .systemLibrary(
            name: "shards",
            pkgConfig: "shards",
            providers: [
                .brew(["shards"]),
                .apt(["libshards-dev"])
            ]
        ),
        .target(
            name: "Shards",
            dependencies: ["shards"],
            path: "include/shards",
            sources: ["shards.swift"]
        ),
    ]
)