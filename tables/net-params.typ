#let net-params-table = {
  let border = rgb("b7b7b7")

  table(
    columns: (auto, auto, auto, auto),
    stroke: border,
    inset: 8pt,
    align: horizon,

    // Header
    table.header(
      [Участок сети], [Пропускная способность], [Задержка], [Размер очереди]
    ),

    // Row 1
    [отправитель-балансировщик_1],
    [10Gbps],
    [1ms],
    [$2.5"МБ"$],

    // Row 2
    [балансировщик_1-маршрутизатор_i],
    [$"data_rate"$],
    [$"delay"$],
    [$2 dot "BDP"$],

    // Row 3
    [маршрутизатор_i-балансировщик_2],
    [10Gbps],
    [1ms],
    [$2.5"МБ"$],

    // Row 4
    [балансировщик_2-получатель],
    [10Gbps],
    [1ms],
    [$2.5"МБ"$],
  )
}
