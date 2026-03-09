#set page(
  paper: "a4",
  margin: (top: 2cm, bottom: 2cm, left: 1.5cm, right: 1.5cm),
)

#set text(
  font: "Times New Roman",
  size: 14pt,
)

#set par()

#set heading(numbering: "1.1.")

#align(center)[
  #text()[
    #image("images/msu.png")
    Московский государственный университет имени М.В. Ломоносова
    Факультет вычислительной математики и кибернетики
    Кафедра автоматизации систем вычислительных комплексов
  ]

  #v(50pt)

  #text(size: 18pt)[Ефанов Михаил Михайлович]

  #text(size: 18pt, weight: "bold")[
    Алгоритм управления перегрузкой на основе пропускной способности и задержки в условиях применения пакетной балансировки
  ]

  #v(50pt)

  #text()[ВЫПУСКНАЯ КВАЛИФИКАЦИОННАЯ РАБОТА]

  #v(100pt)

  #align(right)[
    #text(weight: "bold")[Научный руководитель:]
    #linebreak()
    д.ф.-м.н., профессор, чл.-корр. РАН Смелянский Руслан Леонидович
  ]

  #align(right)[
    #text(weight: "bold")[Научный консультант:]
    #linebreak()
    к.ф.-м.н.,ассистент Степанов Евгений Павлович
  ]

  #align(bottom)[Москва, 2026]
]

#pagebreak()


#heading(outlined: false, numbering: none)[Аннотация]

#v(30pt)

Работа посвящена разработке алгоритма управления перегрузкой на основе пропускной способности и задержки (BBR), адаптированного для работы в условиях пакетной балансировки. Пакетная балансировка значительно повышает производительность и надежность сетевых сервисов за счет горизонтального масштабирования, однако классические алгоритмы управления перегрузкой не адаптированы для совместной работы с ней.

Основные проблемы, возникающие при использовании пакетной балансировки, включают переупорядочивание пакетов и перегрузку каналов, которaя приводит к ложным сигналам перегрузки и снижению скорости передачи данных. В работе проведен обзор существующих решений, направленных на повышение устойчивости алгоритмов управления перегрузкой к переупорядочиванию пакетов и перегрузке каналов: TCP-NCR, Lazy TCP (LTCP) и MSwift.

На основе сравнительного анализа для модификации алгоритма BBR выбран подход, используемый в MSwift, который обеспечивает устойчивость как к переупорядочиванию пакетов, так и к перегрузке каналов. Разработан стенд для моделирования пакетной балансировки в системе ns3, позволяющий исследовать поведение алгоритмов в условиях асимметрии каналов.

#pagebreak()


#outline(
  title: "Оглавление",
)

#pagebreak()


= Введение

#pagebreak()

= Литература

- BBR: Congestion-Based Congestion Control / Neal Cardwellyuchung, Chengc Stephen, Gunnsoheil Hassas, Yeganehvan Jacobson - Queue, 2016 - 34 с. <BBR>
- On the Impact of Packet Spraying in Data Center Networks / Advait Dixit, Pawan Prakash, Y. Charlie Hu, and Ramana Rao Kompella - INFOCOM, 2013 - 9 с. <Impact-of-Packet-Spraying>
- Improving the Robustness of TCP to Non-Congestion Events / S. Bhandarkar, A. L. N. Reddy, M. Allman, E. Blanton - Computer Communications, 2006 - 18 c. <TCP-NCR>
- Making congestion control robust to per-packet load balancing in datacenters - Barak Gerstein, Mark Silberstein, Isaac Keslassy - arXiv, 2025 - 10 c. <MTCP>
- Improving datacenter throughput and robustness with Lazy TCP over packet spraying / Jie Zhanga, Dafang Zhanga, Kun Huang - Computer Communications, 2015 - 11 c. <LTCP>
