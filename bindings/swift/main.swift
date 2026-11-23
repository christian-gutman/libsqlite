// Entry point of the canonical example.
//
// Swift only allows executable statements at the top level of a file named
// main.swift, so ExampleFull.swift wraps the flow in exampleMain() and this
// file only launches it and propagates its exit code (the number of failed
// checks).

import Foundation

do {
    exit(try exampleMain())
} catch {
    print("error: \(error)")
    exit(2)
}