(script_element
  (raw_text) @injection.content
  (#set! injection.language "javascript"))

(component
  (component_script) @injection.content
  (#set! injection.language "javascript"))

(scss_style_element
  (raw_text) @injection.content
  (#set! injection.language "scss"))

(css_style_element
  (raw_text) @injection.content
  (#set! injection.language "css"))

(riot_expression
  (riot_expression_text) @injection.content
  (#set! injection.language "javascript"))

(riot_each_expression
  collection: (riot_each_collection_expression) @injection.content
  (#set! injection.language "javascript"))

(riot_class_pair
  condition: (riot_class_condition) @injection.content
  (#set! injection.language "javascript"))
