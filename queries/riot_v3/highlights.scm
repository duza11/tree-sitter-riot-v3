; Tags
(tag_name) @tag

(start_tag "<" @punctuation.bracket)
(start_tag ">" @punctuation.bracket)
(end_tag "</" @punctuation.bracket)
(end_tag ">" @punctuation.bracket)
(self_closing_element "<" @punctuation.bracket)
(self_closing_element "/>" @punctuation.bracket)
(void_element "<" @punctuation.bracket)
(void_element ">" @punctuation.bracket)
(script_start_tag "<" @punctuation.bracket)
(script_start_tag ">" @punctuation.bracket)
(script_end_tag "</" @punctuation.bracket)
(script_end_tag ">" @punctuation.bracket)
(scss_style_start_tag "<" @punctuation.bracket)
(scss_style_start_tag ">" @punctuation.bracket)
(css_style_start_tag "<" @punctuation.bracket)
(css_style_start_tag ">" @punctuation.bracket)
(style_end_tag "</" @punctuation.bracket)
(style_end_tag ">" @punctuation.bracket)

; Attributes
(attribute_name) @attribute
(quoted_attribute_value) @string
(unquoted_attribute_value) @string

((attribute_name) @keyword.directive
  (#any-of? @keyword.directive
    "if" "show" "hide" "no-reorder" "riot-tag" "riot-src" "riot-value" "riot-style" "ref"))

(each_attribute
  name: (attribute_name) @keyword.repeat)

(quoted_riot_each_expression
  ["\"" "'"] @string)

((attribute_name) @function
  (#match? @function "^on[a-zA-Z]+$"))

; Comments
(comment) @comment

; Riot expressions
(riot_expression "{" @punctuation.bracket)
(riot_expression "}" @punctuation.bracket)
(riot_expression_text) @variable
(riot_each_expression "{" @punctuation.bracket)
(riot_each_expression "}" @punctuation.bracket)
(riot_each_expression_text) @variable
(riot_class_expression "{" @punctuation.bracket)
(riot_class_expression "}" @punctuation.bracket)
(riot_class_expression_text) @variable
