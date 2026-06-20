package tree_sitter_riot_v3_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_riot_v3 "github.com/duza11/tree-sitter-riot-v3/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_riot_v3.Language())
	if language == nil {
		t.Errorf("Error loading Riot.js v3 grammar")
	}
}
