/**
 * @file Riot.js v3 grammar for tree-sitter
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const WHITESPACE = /[\s\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\ufeff]+/;
const UNQUOTED_ATTRIBUTE_TEXT = /[^\s\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\ufeff<>'"{}]+/;

module.exports = grammar({
  name: 'riot_v3',

  extras: _ => [
    WHITESPACE,
  ],

  externals: $ => [
    $.raw_text,
    $.component_script,
    $.riot_expression_text,
    $._riot_each_shorthand_expression_text,
    $.riot_each_collection_expression,
    $.riot_class_identifier_name,
    $.riot_class_string_name,
    $.riot_class_condition,
    $._scss_style_tag_name,
    $._css_style_tag_name,
    $._script_start_tag_name,
    $._start_tag_name,
    $._void_start_tag_name,
    $._end_tag_name,
    $.erroneous_end_tag_name,
    $._implicit_end_tag,
    '/>',
  ],

  rules: {
    source_file: $ => repeat($._top_level_node),

    _top_level_node: $ => choice(
      $.component,
      $.comment,
      $.erroneous_end_tag,
      $.text,
    ),

    component: $ => seq(
      $.start_tag,
      repeat($._component_child),
      optional($.component_script),
      $.end_tag,
    ),

    _component_child: $ => choice(
      $.script_element,
      $._style_element,
      $.comment,
      $.erroneous_end_tag,
      $.element,
    ),

    _template_node: $ => choice(
      $.script_element,
      $._style_element,
      $.comment,
      $.erroneous_end_tag,
      $.element,
      $.riot_expression,
      $.text,
    ),

    element: $ => choice(
      $.normal_element,
      $.self_closing_element,
      $.void_element,
    ),

    normal_element: $ => seq(
      $.start_tag,
      repeat($._template_node),
      choice($.end_tag, $._implicit_end_tag),
    ),

    self_closing_element: $ => seq(
      '<',
      field('name', alias(choice($._start_tag_name, $._void_start_tag_name), $.tag_name)),
      repeat($._attribute),
      '/>',
    ),

    void_element: $ => seq(
      '<',
      field('name', alias($._void_start_tag_name, $.tag_name)),
      repeat($._attribute),
      '>',
    ),

    start_tag: $ => seq(
      '<',
      field('name', alias($._start_tag_name, $.tag_name)),
      repeat($._attribute),
      '>',
    ),

    end_tag: $ => seq(
      '</',
      field('name', alias($._end_tag_name, $.tag_name)),
      '>',
    ),

    erroneous_end_tag: $ => seq(
      '</',
      $.erroneous_end_tag_name,
      '>',
    ),

    script_element: $ => seq(
      $.script_start_tag,
      optional($.raw_text),
      $.script_end_tag,
    ),

    script_start_tag: $ => seq(
      '<',
      field('name', alias($._script_start_tag_name, $.tag_name)),
      repeat($._attribute),
      '>',
    ),

    script_end_tag: $ => seq(
      '</',
      alias($._end_tag_name, $.tag_name),
      '>',
    ),

    _style_element: $ => choice(
      $.scss_style_element,
      $.css_style_element,
    ),

    scss_style_element: $ => seq(
      $.scss_style_start_tag,
      optional($.raw_text),
      $.style_end_tag,
    ),

    css_style_element: $ => seq(
      $.css_style_start_tag,
      optional($.raw_text),
      $.style_end_tag,
    ),

    scss_style_start_tag: $ => seq(
      '<',
      field('name', alias($._scss_style_tag_name, $.tag_name)),
      repeat($._attribute),
      '>',
    ),

    css_style_start_tag: $ => seq(
      '<',
      field('name', alias($._css_style_tag_name, $.tag_name)),
      repeat($._attribute),
      '>',
    ),

    style_end_tag: $ => seq(
      '</',
      alias($._end_tag_name, $.tag_name),
      '>',
    ),

    _attribute: $ => choice(
      $.each_attribute,
      $.attribute,
    ),

    each_attribute: $ => seq(
      field('name', alias('each', $.attribute_name)),
      '=',
      field('value', choice(
        $.unquoted_riot_each_attribute_value,
        $.quoted_riot_each_expression,
      )),
    ),

    unquoted_riot_each_attribute_value: $ => seq(
      choice($.riot_each_expression, $.unquoted_attribute_text),
      repeat(choice(
        $.riot_each_expression,
        alias($._unquoted_attribute_text_immediate, $.unquoted_attribute_text),
      )),
    ),

    quoted_riot_each_expression: $ => choice(
      seq('"', $.riot_each_expression, '"'),
      seq('\'', $.riot_each_expression, '\''),
    ),

    attribute: $ => seq(
      $.attribute_name,
      optional(seq('=', $.attribute_value)),
    ),

    attribute_name: _ => /[A-Za-z_:][A-Za-z0-9_:.-]*/,

    attribute_value: $ => choice(
      $.quoted_attribute_value,
      $.unquoted_attribute_value,
    ),

    quoted_attribute_value: $ => choice(
      seq('"', repeat(choice($.riot_class_expression, $.riot_expression, $.quoted_attribute_text)), '"'),
      seq('\'', repeat(choice($.riot_class_expression, $.riot_expression, $.single_quoted_attribute_text)), '\''),
    ),

    quoted_attribute_text: _ => /[^"{}]+/,

    single_quoted_attribute_text: _ => /[^'{}]+/,

    unquoted_attribute_value: $ => seq(
      choice($.riot_class_expression, $.riot_expression, $.unquoted_attribute_text),
      repeat(choice(
        $.riot_class_expression,
        $.riot_expression,
        alias($._unquoted_attribute_text_immediate, $.unquoted_attribute_text),
      )),
    ),

    unquoted_attribute_text: _ => token(prec(-1, UNQUOTED_ATTRIBUTE_TEXT)),

    _unquoted_attribute_text_immediate: _ => token.immediate(prec(1, UNQUOTED_ATTRIBUTE_TEXT)),

    tag_name: _ => token(prec(1, /[A-Za-z][A-Za-z0-9_:.-]*/)),

    script_tag_name: _ => token(prec(10, /[sS][cC][rR][iI][pP][tT]/)),

    style_tag_name: _ => token(prec(10, /[sS][tT][yY][lL][eE]/)),

    comment: _ => token(seq(
      '<!--',
      optional(/[^-]*(-[^->][^-]*)*/),
      '-->',
    )),

    riot_expression: $ => seq(
      '{',
      optional($.riot_expression_text),
      '}',
    ),

    riot_each_expression: $ => seq(
      '{',
      choice(
        field('collection', alias(
          $._riot_each_shorthand_expression_text,
          $.riot_each_collection_expression,
        )),
        seq(
          field('binding', $.riot_each_binding),
          optional(seq(',', field('index', $.riot_each_binding))),
          field('operator', alias('in', $.riot_each_operator)),
          field('collection', $.riot_each_collection_expression),
        ),
      ),
      '}',
    ),

    riot_each_binding: _ => /[A-Za-z_$][A-Za-z0-9_$]*/,

    riot_class_expression: $ => seq(
      '{',
      $.riot_class_pair,
      repeat(seq(',', $.riot_class_pair)),
      '}',
    ),

    riot_class_pair: $ => seq(
      field('name', $.riot_class_name),
      field('operator', alias(':', $.riot_class_operator)),
      field('condition', $.riot_class_condition),
    ),

    riot_class_name: $ => choice(
      $.riot_class_identifier_name,
      $.riot_class_string_name,
    ),

    text: _ => token(prec(1, repeat1(choice(
      /[^<{\s\u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\ufeff][^<{]*/,
      seq('<', /[^A-Za-z/!-]/),
      seq('<-', /[^A-Za-z]/),
      seq('</', /[^A-Za-z-]/),
      seq('</-', /[^A-Za-z]/),
      seq('<!', /[^-]/),
      seq('<!-', /[^-]/),
    )))),
  },
});
