import construct as ct
import gleam/io
import gleam/option

pub fn main() -> Nil {
  giant_module
  |> ct.to_string(0)
  |> io.println()
}


const giant_comment =
  "This module intentionally contains extremely deep nesting, " <>
  "multiple scoped expressions, repeated function declarations, " <>
  "and formatter-heavy structures in order to stress test rendering."

const nested_metrics_block =
  ct.ScopedExpr([
    ct.Comment(
      ct.SingleLine,
      "Level 1 metrics aggregation block",
    ),

    ct.Let(
      "cpu_usage",
      option.Some("Float"),
      ct.Literal("91.24"),
    ),

    ct.Let(
      "memory_usage",
      option.Some("Float"),
      ct.Literal("73.11"),
    ),

    ct.ScopedExpr([
      ct.Comment(
        ct.SingleLine,
        "Level 2 nested analysis scope",
      ),

      ct.Let(
        "disk_usage",
        option.Some("Float"),
        ct.Literal("88.8"),
      ),

      ct.Let(
        "network_usage",
        option.Some("Float"),
        ct.Literal("44.2"),
      ),

      ct.ScopedExpr([
        ct.Comment(
          ct.SingleLine,
          "Level 3 nested diagnostics",
        ),

        ct.Let(
          "deep_value_a",
          option.Some("Int"),
          ct.Literal("1000"),
        ),

        ct.Let(
          "deep_value_b",
          option.Some("Int"),
          ct.Literal("2000"),
        ),

        ct.Let(
          "deep_value_c",
          option.Some("Int"),
          ct.ScopedExpr([
            ct.Literal("deep_value_a"),
            ct.Literal("deep_value_b"),
            ct.Literal("999999"),
          ]),
        ),

        ct.Function(
          "deep_analyzer",
          option.Some("String"),
          [
            ct.FuncParam(
              "dataset",
              option.Some("List(String)"),
            ),
            ct.FuncParam(
              "iterations",
              option.Some("Int"),
            ),
            ct.FuncParam(
              "strict",
              option.Some("Bool"),
            ),
            ct.FuncParam(
              "environment",
              option.Some("String"),
            ),
          ],
          [
            ct.Comment(
              ct.SingleLine,
              "Extremely nested analyzer body",
            ),

            ct.Let(
              "header",
              option.Some("String"),
              ct.ScopedExpr([
                ct.Literal("\"Dataset:\""),
                ct.Literal("environment"),
                ct.Literal("\" -> iterations=\""),
                ct.Literal("iterations"),
              ]),
            ),

            ct.ScopedExpr([
              ct.Let(
                "inner_counter",
                option.Some("Int"),
                ct.Literal("0"),
              ),

              ct.Let(
                "pipeline",
                option.Some("Pipeline"),
                ct.ScopedExpr([
                  ct.Literal("\"stage_1\""),
                  ct.Literal("\"stage_2\""),
                  ct.Literal("\"stage_3\""),
                  ct.Literal("\"stage_4\""),
                  ct.Literal("\"stage_5\""),
                ]),
              ),

              ct.Function(
                "recursive_formatter",
                option.Some("String"),
                [
                  ct.FuncParam(
                    "input",
                    option.Some("String"),
                  ),
                  ct.FuncParam(
                    "depth",
                    option.Some("Int"),
                  ),
                ],
                [
                  ct.Let(
                    "formatted",
                    option.Some("String"),
                    ct.ScopedExpr([
                      ct.Literal("\"depth=\""),
                      ct.Literal("depth"),
                      ct.Literal("\" value=\""),
                      ct.Literal("input"),
                    ]),
                  ),

                  ct.Literal("formatted"),
                ],
              ),
            ]),

            ct.Literal("header"),
          ],
        ),
      ]),
    ]),
  ])

const giant_function =
  ct.Function(
    "orchestrate_massive_render_pipeline",
    option.Some("RenderResult"),
    [
      ct.FuncParam(
        "config",
        option.Some("RenderConfig"),
      ),
      ct.FuncParam(
        "input_files",
        option.Some("List(String)"),
      ),
      ct.FuncParam(
        "cache_enabled",
        option.Some("Bool"),
      ),
      ct.FuncParam(
        "retry_limit",
        option.Some("Int"),
      ),
      ct.FuncParam(
        "environment",
        option.Some("String"),
      ),
      ct.FuncParam(
        "telemetry",
        option.Some("TelemetryConfig"),
      ),
    ],
    [
      ct.Comment(
        ct.MultiLine,
        "Massive orchestration function body.\n" <>
        "This exists to stress indentation,\n" <>
        "spacing, function rendering,\n" <>
        "nested scope rendering,\n" <>
        "and literal formatting.",
      ),

      ct.Let(
        "pipeline_name",
        option.Some("String"),
        ct.Literal("\"omega-render-pipeline\""),
      ),

      ct.Let(
        "boot_message",
        option.Some("String"),
        ct.ScopedExpr([
          ct.Literal("\"Booting pipeline for environment=\""),
          ct.Literal("environment"),
          ct.Literal("\" with retry limit=\""),
          ct.Literal("retry_limit"),
        ]),
      ),

      nested_metrics_block,

      ct.ScopedExpr([
        ct.Comment(
          ct.SingleLine,
          "Deep orchestration scope",
        ),

        ct.Let(
          "phase_1",
          option.Some("String"),
          ct.Literal("\"collect\""),
        ),

        ct.Let(
          "phase_2",
          option.Some("String"),
          ct.Literal("\"transform\""),
        ),

        ct.Let(
          "phase_3",
          option.Some("String"),
          ct.Literal("\"serialize\""),
        ),

        ct.Let(
          "phase_4",
          option.Some("String"),
          ct.Literal("\"persist\""),
        ),

        ct.Function(
          "emit_diagnostics",
          option.Some("Diagnostics"),
          [
            ct.FuncParam(
              "session_id",
              option.Some("String"),
            ),
            ct.FuncParam(
              "verbose",
              option.Some("Bool"),
            ),
          ],
          [
            ct.Let(
              "diag_payload",
              option.Some("String"),
              ct.ScopedExpr([
                ct.Literal("\"diagnostics:\""),
                ct.Literal("session_id"),
                ct.Literal("\" verbose=\""),
                ct.Literal("verbose"),
              ]),
            ),

            ct.Literal("diag_payload"),
          ],
        ),
      ]),

      ct.Literal("pipeline_name"),
    ],
  )

const giant_module =
  ct.Module([
    ct.Comment(
      ct.SingleLine,
      giant_comment,
    ),

    ct.Comment(
      ct.MultiLine,
      "Formatter benchmark suite\n" <>
      "Contains deeply recursive rendering\n" <>
      "patterns and nested structures.",
    ),

    ct.Let(
      "application_name",
      option.Some("String"),
      ct.Literal("\"ether_formatter_benchmark\""),
    ),

    ct.Let(
      "version",
      option.Some("String"),
      ct.Literal("\"9.8.1\""),
    ),

    ct.Let(
      "build_mode",
      option.Some("String"),
      ct.Literal("\"stress-test\""),
    ),

    ct.Let(
      "global_settings",
      option.Some("GlobalSettings"),
      ct.ScopedExpr([
        ct.Let(
          "max_threads",
          option.Some("Int"),
          ct.Literal("64"),
        ),

        ct.Let(
          "max_memory",
          option.Some("Int"),
          ct.Literal("32768"),
        ),

        ct.Let(
          "enable_disk_cache",
          option.Some("Bool"),
          ct.Literal("True"),
        ),

        ct.Let(
          "enable_streaming",
          option.Some("Bool"),
          ct.Literal("True"),
        ),

        nested_metrics_block,
      ]),
    ),

    giant_function,

    giant_function,

    ct.Function(
      "final_export",
      option.Some("String"),
      [
        ct.FuncParam(
          "target",
          option.Some("String"),
        ),
        ct.FuncParam(
          "artifacts",
          option.Some("List(String)"),
        ),
      ],
      [
        ct.Let(
          "export_message",
          option.Some("String"),
          ct.ScopedExpr([
            ct.Literal("\"Exporting to: \""),
            ct.Literal("target"),
          ]),
        ),

        giant_function,

        ct.Literal("export_message"),
      ],
    ),
  ])


