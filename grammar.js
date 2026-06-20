/**
 * @file Riot.js v3 grammar for tree-sitter
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'riot_v3',

  extras: _ => [
    /[\s\u3000]+/,
  ],

  externals: $ => [
    $.raw_text,
    $.component_script,
    $.riot_expression_text,
    $.riot_each_expression_text,
    $.riot_class_expression_text,
    $._scss_style_tag_name,
    $._css_style_tag_name,
    $._script_start_tag_name,
    $._start_tag_name,
    $._void_start_tag_name,
  ],

  rules: {
    source_file: $ => repeat($._top_level_node),

    _top_level_node: $ => choice(
      $.component,
      $.comment,
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
      $.element,
    ),

    _template_node: $ => choice(
      $.script_element,
      $._style_element,
      $.comment,
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
      $.end_tag,
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
      field('name', $.tag_name),
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
      alias($.script_tag_name, $.tag_name),
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
      alias($.style_tag_name, $.tag_name),
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
        $.riot_each_expression,
        $.quoted_riot_each_expression,
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
      $.riot_class_expression,
      $.riot_expression,
      $.unquoted_attribute_value,
    ),

    quoted_attribute_value: $ => choice(
      seq('"', repeat(choice($.riot_class_expression, $.riot_expression, $.quoted_attribute_text)), '"'),
      seq('\'', repeat(choice($.riot_class_expression, $.riot_expression, $.single_quoted_attribute_text)), '\''),
    ),

    quoted_attribute_text: _ => /[^"{}]+/,

    single_quoted_attribute_text: _ => /[^'{}]+/,

    unquoted_attribute_value: _ => token(prec(-1, /[^\s<>'"={}]+/)),

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
      $.riot_each_expression_text,
      '}',
    ),

    riot_class_expression: $ => seq(
      '{',
      $.riot_class_expression_text,
      '}',
    ),

    text: _ => token(prec(-1, /[^<{\s\u3000][^<{]*/)),
  },
});
