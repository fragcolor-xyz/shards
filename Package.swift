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
        .target(
            name: "shards",
            path: "include/shards",
            sources: [],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath(".")
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