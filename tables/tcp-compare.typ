#let tcp-compare-table = {
  let green = rgb("d9ead3")
  let red = rgb("f4cccc")
  let orange = rgb("fce5cd")
  let border = rgb("b7b7b7")

  table(
    columns: (auto, auto, auto, auto),
    stroke: border,
    inset: 8pt,
    align: horizon,

    // Header
    table.header(
      [], [TCP-NCR], [LTCP], [MSwift]
    ),

    fill: (x, y) => {
      if y == 0 { if x == 0 { none } }
      if y == 0 { if x == 1 { green } }
      if y == 0 { if x == 2 { red } }
      if y == 0 { if x == 3 { red } }
      else if x == 0 { none }
      else if y == 1 { green }
      else if y == 2 { if x == 1 or x == 2 { red } else { green } }
      else if y == 3 { if x == 1 { green } else { red } }
      else if y == 4 { if x == 1 { green } else { red } }
      else { none }
    },

    // Row 1
    [Сохранение скорости передачи при переупорядочивании пакетов],
    [Да],
    [Да],
    [Да],

    // Row 2
    [Сохранение скорости передачи при ассиметрии каналов],
    [Нет],
    [Нет],
    [Да],

    // Row 3
    [Независимость от стратегии балансировки],
    [Да],
    [Нет],
    [Нет],

    // Row 5
    [Применимость к BBR],
    [Да],
    [Нет],
    [Нет],
  )
}
