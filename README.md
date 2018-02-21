﻿# seismix #

Light scriptable sounds player.

## Project description

Всем привет. Представляю вашему взору одну прогу, которую я написал в 2011 году под заказ какому-то
чуваку всего за 100 баксов. Я тут смотрю сейчас на это дело критически, и считаю что продешевил,
надо было больше с этого срубить. Но когда я увидел задачу, мне тогда показалось, что она и выединного
яйца не стоит. В принципе так и есть. Суть задачи была в том, чтобы сделать программируемый плеер коротких
звуков. Каждый звук сопоставляется с идентификатором, плейлист представляет собой непрерывную
последовательность идентификаторов. Реализация заняла где-то около недели, параллельно с основной работой.
На полученные бабки я потом неплохо погулял с одной тёлкой, есть что вспомнить. Тёлка была жирная,
но всё равно было прикольно, золотое было время.

Ну в общем, воспроизведение я сделал через стандартный виндовый интерфейс MCI, который реализуется там ещё
до всякого directx. Можно было и под DX, только смысла в этом нет, никаких манипуляций со стримом нет,
а MCI избавляет от лишних зависимостей, DX ставился тогда отдельно, и позволяет MCI воспроизводить разные
аудио форматы без лишнего гемора с directshow. Вызовы MCI я заскриптовал под Lua. По-моему вышло всё
очень клёво.

## Issues

Название seismix дал поскольку слушал тогда альбом sasha & john digweed - communicate (cd2), там есть
такой трек. Хотя следующий трек ещё прикольнее. И дальше - тоже.

Изначально проект был написан под VS2008, но поскольку эта версия студии уже безнадёжно устарела, я тут
перевёл всё под VS2017, и немного подчистил код, чтобы всё собиралось без проблем. Поставил актуальную версию
Lua 5.3.4, до этого была 5.1.4.

Тут наверное некоторые чешут репу на тему, почему я это описалово пишу на русском, а не на инглише. Дело
не в скрепах, просто всё равно все ресурсы, документация и скрипты на русском, поскольку заказчик местный.
Так что китайские коллеги даже если сюда зайдут, всё равно встанут перед фактом.

Ещё наверное кто-то пребывает в лёгком конфузе от стилистики описалова, расходящимся с общим трендом на гитхабе.
Знаете, если заняться критической самооценкой, я написал тонны всякого бреда в разной документации как
канцелярским, так и техническим языком, которая всё равно никому нафиг не нужна. Читайте прилагаемый мануал.
С этой программы я бабки всё равно никогда больше не получу, так что всё по барабану. Да, и ещё я рад за вас,
если вы всё это прочли.

## License

Даже не знаю, что тут написать. Ну юзайте по [MIT](http://www.opensource.org/licenses/MIT) лицензии,
не один ли хрен. Если ещё прога кому-то нужна.

Автор: &copy; Beowulf (xbeowolf@gmail.com)

P. S. Если кто-то проникся проектом и хочет обсудить, пейшите на мыло, или на скайп x-beowulf. Если не
прониклись проектом, и есть желание обсудить - тоже пишите, обсудим. Ну или может если у кого-то вдруг
возникло непреодолимое желание забашлять мне за какой-нибудь новый проект, мало ли какие в жизни ситуации
бывают - ясное дело тоже пишите.