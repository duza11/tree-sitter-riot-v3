import XCTest
import SwiftTreeSitter
import TreeSitterHTML

final class TreeSitterHTMLTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_riot_v3())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Riot.js v3 grammar")
    }
}
