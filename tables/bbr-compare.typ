#let bbr-compare-table = {
  let green = rgb("d9ead3")
  let red = rgb("f4cccc")
  let border = rgb("b7b7b7")

  table(
    columns: (auto, auto, auto, auto),
    stroke: border,
    inset: 8pt,
    align: horizon,

    // Header
    table.header(
      [], [BBRv1], [BBRv2], [BBRv3]
    ),

    fill: (x, y) => {
      if y == 0 { none }
      if y == 1 { if x == 0 { none } else if x == 1 { green } else { red } }
      if y == 2 { if x == 0 { none } else if x == 1 or x == 2 { green } else { red } }
    },

    // Row 1
    [Отсутствие реакции на потерю пакетов],
    [Да],
    [Нет],
    [Нет],

    // Row 2
    [Реализация в NS-3],
    [Да],
    [Да],
    [Нет],
  )
}
