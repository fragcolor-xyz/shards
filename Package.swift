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
            name: "Shards",
            path: "include/shards",
            sources: ["shards.swift"]
        ),
    ]
)