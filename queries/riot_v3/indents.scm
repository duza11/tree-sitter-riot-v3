((component
  (start_tag
    (tag_name) @_not_void_element))
  (#not-any-of? @_not_void_element
    "area" "base" "basefont" "bgsound" "br" "col" "command" "embed" "frame" "hr" "image" "img"
    "input" "isindex" "keygen" "link" "menuitem" "meta" "nextid" "param" "source" "track" "wbr")) @indent.begin

((normal_element
  (start_tag
    (tag_name) @_not_void_element))
  (#not-any-of? @_not_void_element
    "area" "base" "basefont" "bgsound" "br" "col" "command" "embed" "frame" "hr" "image" "img"
    "input" "isindex" "keygen" "link" "menuitem" "meta" "nextid" "param" "source" "track" "wbr")) @indent.begin

(self_closing_element) @indent.begin

; A self-closing child can put the surrounding component into ERROR while editing.
(ERROR
  (start_tag)
  (element
    (self_closing_element))) @indent.begin

((void_element
  (tag_name) @_void_element)
  (#any-of? @_void_element
    "area" "base" "basefont" "bgsound" "br" "col" "command" "embed" "frame" "hr" "image" "img"
    "input" "isindex" "keygen" "link" "menuitem" "meta" "nextid" "param" "source" "track" "wbr")) @indent.begin

; These are the nodes that will be captured when we do `normal o`.
; But last element has already been ended, so capturing this
; marks the end of the last element.
(component
  (end_tag
    ">" @indent.end))

(normal_element
  (end_tag
    ">" @indent.end))

(self_closing_element
  "/>" @indent.end)

; Script/style elements are injected into other languages, so only branch
; the end tags of template/component elements.
(component
  (end_tag) @indent.branch)

(normal_element
  (end_tag) @indent.branch)

(erroneous_end_tag) @indent.branch

[
  ">"
  "/>"
] @indent.branch

(comment) @indent.ignore
