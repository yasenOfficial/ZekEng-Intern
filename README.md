# Цел на стажа:

Да се направи симулация на вече направена платка за контролиране на вентилатор. Платката гори често и има нужда от оптимизация. Дадени са файлове за Reverse-engineering в `/Files for Reverse Engenieering`

![TaskPCB2](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/TaskPCB2.jpg)
![TaskPCB1](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/TaskPCB1.jpg)

# Ден 1 - 01.07.2025

Запознанство с екипа

Разучаване LTSpice от огромния Роман (Следване на Tutorial-a)

Тutorial-a.......
![Tutorial Book](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/Book.jpg)

Работна директория: `LTSpice Files/TutorialRoman.asc`

Покрито в Tutorial-a:
- Поставяне на компоненти
- Свързване
- Поставяне на стойности на компонентите
- Директиви
- Анализ на сигнали

Описание на директивите

## Честотен анализ (.ac)

Изпраща много бавен и много бърз синус (от 1 kHz до 10 MHz) през схемата и измерва колко усилва тя и с колко закъснява сигналът.

`.ac dec 50 1k 10Meg`

dec 50 – логаритмична скала с 50 точки за всяка декада. Т.е. първо разглеждаме честоти от 1 kHz до 10 kHz (1–10 kHz), после от 10 kHz до 100 kHz, после от 100 kHz до 1 MHz и т.н.

1k – начална честота 1 kHz

10Meg – крайна честота 10 MHz

![ACanalysis](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/ACAnalysis.png)

## Транзиентен анализ (.tran)

Проследява как се променят напреженията и токовете във времето (от 0 до 300 µs), записвайки данни след първите 100 µs.

`.tran 0 300u 100u 0.01u`

0 – автоматичен интервал за записване на резултатите

300u – крайно време на симулация 300 µs

100u – начало на записването от 100 µs

0.01u – максимална стъпка 0.01 µs

![TransitiveAnalysis](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/TransitiveAnalysis.png)

## Фурие-анализ (.four)

- Взима резултата от временния анализ и го разгражда на отделни синусови вълни (хармоници), за да видим колко „чист“ е сигналът на 500 kHz.

`.four 500k 10V(out)`

500k – основна честота 500 kHz за хармоничен анализ

10V(out) – (10 V при основния хармоник на изхода)

![FFTAnalysis](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/FFTAnalysis.png)


## Параметрично стъпково преминаване (.step)

-  Променя автоматично стойността на резистор Rgain (100 kΩ, 120 kΩ, 140 kΩ… до 200 kΩ) и за всяка стойност пуска пак AC, транзиентен и Фурие-анализите.

`.step lin param Rgain 100k 200k 20k`

Променя стойността на параметъра Rgain от 100 kΩ до 200 kΩ на стъпки по 20 kΩ

За всяка стойност се повтарят горните анализи

![Multiple Simulations](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/MultipleSimulations.png)


## Идентифициране на дефект

След стартиране на транзиентен анализ в определен момент изходният сигнал се отклонява от очакваната синусоида:

Наблюдение на графиката: Обръща се внимание къде сигналът изкривява или има рязка промяна на наклона.

![IndentifyingDefect](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/IndentifyingDefect.png)

Измерване с курсори: Поставят се два курсора преди и в началото на аномалията, за да се изчисли промяната във времето и напрежението.

На операционния усилвател му отнема измеримо време за да покрие съществуващия пад на напреженеие между сумирания VBE на 2та транзистора Q1, Q2

Извод: 3тия сигнал (от 6-те) е минималния приемлив (R1 = 140k) с амплитуда 14V



# Ден 2 - 02.07.2025

## Импортиране на CD4000 серията в LTSpice

`Ventilator\lib\sym\*` `Ventilator\lib\sub\CD4000.lib` 

https://www.amarketplaceofideas.com/adding-series-4000-cmos-library-to-ltspice.htm

## Тестове на NAND 4093B gate

`Ventilator\NAND Tests.asc`

Проведох прост дали NAND gate-a от библиотеката работи със синусоида с амплитуда 12 V

![IndentifyingDefect](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/NANDTests.png)


## Тестове и разбиране как работи SW gate-a в LTSpice

Следвах Tutorial-a на Analog Devices:

`Ventilator\SW.asc`

![Switches Demo Implementation](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/SwitchesADDemo.png)

И после направих подобна моя имплементация с резисотри вместо импулсен генератор:

![Switches Custom Implementation](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/SwitchesCustom.png)


## Импортирах и библиотектата с симистора (MAC97A8), който е сложен на платката

`Ventilator\lib\sub\thyristr.lib` 

https://github.com/HMGrunthos/InrushTimer/blob/master/thyristr.lib

## Teстване на захранването на схемата:

`Ventilator\PMU.asc`

Захранвaнето се стабилизира за около 160ms докато стигне 11.3V. После лека полека се покачва до 11.6 в рамките на до 2 секунди

![IndentifyingDefect](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/PMU11.3V.png)
![IndentifyingDefect](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/PMU11.6V.png)

Параметри на проблемния кондензатор:

![C params](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/Cparams.png)

Pulse current: Rated for 2 A rms at 1 kHz → Доста повече от 1.2 A
https://www.mouser.bg/ProductDetail/WIMA/FKP1R022205H00KYSD?qs=sGAEpiMZZMsh%252B1woXyUXj5VlawBRZJwFPjr6nKaRZtw%3D

Параметри на проблемните резистори:

![R params](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/RParams.png)

https://www.mouser.bg/ProductDetail/ROHM-Semiconductor/ESR18EZPJ131?qs=493kPxzlxfJ2i%2Fymb6BOsw%3D%3D

# Ден 3 - 03.07.2025

Започната е работа по схемата:

![SchematicFanV1](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/SchematicFanV1.png)

- сменен е 220nF-овия кондензатор, избрания от вчера (02.07.2025 се оказва голям за платка (трябва да се събера в 14.5mm x 32.5mm))

https://www.mouser.bg/ProductDetail/WIMA/MKP4J022203C00KI00?qs=sJjjjplDs9tWJMp1Tfr4Pw%3D%3D

- сменена е топологията на резисторите (вече са последователни, а не паралелни)
- избран е нов ценеров диод, тъй като стария е неизвестен

Започната е работа по платката:

![PCBFanV1](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/PCBFanV1.png)

Начерах Custom TP, Diameter 2.0mm, drill 1.0mm, Pad само на свързана страна
![TestPoint_Top_Hole_Only_D2.0mm](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/TestPoint_Top_Hole_Only_D2.0mm.png)


Платката и схемата за предадени за обратна връзка на екипа от ЗекЕнг, със сигурност трябва да се добави медна зона за земя (GND) и да се оправи place-ment-a на резисторите R21, R22, R23, R24
![PCBFanV1.1](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/PCBFanV1.1.png)

(Това е много бърза версия на платката)

Oбратна връзка:

- Да се начертае как ще легнат кондензаторите 2x (100uF)
- Да се сложат кондензаторите от другата страна 2x (100uF)
- Да се оправи символа и footprint-a на Hum Sensor-a, за да изглежда по-добре
- Да се оправят R21, R22, R23, R24 (Да се внимава с напрежението там)
- Да се направи TP с 1.8mm Diam. x 1.1 Drill
- Да проверя pinout-a на симистора
- Засега да изчакам с медните зони (може и просто да свържа земята на ръка)
- Очаквам Feedback за избрания кондензатор и ценер

# Ден 4 - 04.07.2025

~~ - Да се начертае как ще легнат кондензаторите 2x (100uF) ~~
~~ - Да се сложат кондензаторите от другата страна 2x (100uF) ~~
~~ - Да се оправи символа и footprint-a на Hum Sensor-a, за да изглежда по-добре ~~
~~ - Да се оправят R21, R22, R23, R24 (Да се внимава с напрежението там) ~~
~~ - Да се направи TP с 1.8mm Diam. x 1.1 Drill ~~
- Да проверя pinout-a на симистора 


- Кондензатора имаше грешка бях избрал 0.022uf (22nf) вместо 0.22uf (220nf)
- Ценера е добре

Платката е приключена, предавам за проверка и обратна връзка:

- начертан е нов footprint за 220nf-вия кондензатор, за да може да support-ва няколко различни корпуса. (3D модела е също съответно променен)
- начертан е нов footprint за 100uf-вите кондензатори, за да се види как ще изглеждат реално като са легнали. (3D модела е също съответно променен)

- компонентите са пренаредени, за да може падовете за връзка с вентилатора да не са под 100uf-вите кондензатори
- сменен е модела на 220nf-вия кондензатор (да не е 22nf) - https://www.mouser.bg/ProductDetail/WIMA/MKP4J032204J00KSSD?qs=sJjjjplDs9tNFMTkeYgn1g%3D%3D

![PCBFanV1.2](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/PCBFanV1.2.png)
![3DFanV1.1](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/3DFanV1.1.png)
![3DFanV1.2](https://github.com/yasenOfficial/ZekEng-Intern/blob/main/Images/3DFanV1.2.png)
