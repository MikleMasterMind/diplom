#let bbrv3-params-table = {
  let border = rgb("b7b7b7")

  table(
    columns: (auto, auto, auto, auto),
    stroke: border,
    inset: 8pt,
    align: horizon,

    // Header
    table.header(
      [Параметр], [BBRv1], [BBRv2], [BBRv3]
    ),
    [Коэффициент StartUp], [$2 / (ln 2)$], [$2 / (ln 2)$], [2.77],
    [Коэффициент ProbeUp], [1.25], [2], [2.25],
    [Коэффициент ProbeDown], [0.75], [0.75], [0.9],
  )
}
