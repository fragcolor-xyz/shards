// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "ShardsSwift",
    platforms: [
        .macOS(.v12),
        .iOS(.v15),
        .visionOS(.v1)
    ],
    products: [
        .library(
            name: "ShardsSwift",
            targets: ["ShardsSwift"]
        ),
    ],
    targets: [
        .target(
            name: "shards_native",
            path: "include/shards",
            sources: [],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath(".")
            ]
        ),
        .target(
            name: "ShardsSwift",
            dependencies: ["shards_native"],
            path: "include/shards",
            sources: ["shards.swift"],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath(".")
            ]
        ),
    ]
)