---UTF8------------------------------------------------------------------------
--                                ATTENTION!                                 --
-- Developer provides this script as is with no warranty of proper work on   --
-- any users modifications. All modifications are carried out at own risk.   --
--                   © BEOWOLF / Podobashev Dmitry, 2011                     --
--     email: xbeowolf@gmail.com     skype: x-beowulf     icq: 320329575     --
-------------------------------------------------------------------------------

-- Редактировать этот скрипт можно в Notepad++ http://notepad-plus-plus.org/

dofile("keyfiles.lua")

JSeismix = {}

-------------------------------------------------------------------------------
-- Startup code and initialization
-------------------------------------------------------------------------------

-- Максимальная длина идентификатора в плейлисте
MaxIdLength = 5

-- Идентификаторы элементов управления графического интерфейса,
-- эти константы определены в исполнимом коде,
-- не изменяйте значения из этого списка без перекомпиляции!!!
idcOk       = 1 -- IDOK
idcCancel   = 2 -- IDCANCEL
idcPlaylist = 110
idcPlay     = 111
idcStop     = 112
idcSkipErr  = 113
idcFiles    = 114
idcOpen     = 115
idcSave     = 116
idcShowPath = 117
idcShowExt  = 118
idcDelete   = 119

-- Сообщения для всплывающих подсказок
TitlePlaylist = "Плейлист"
TitlePlay = "Кнопка \"Иргать/Пауза\""
TitleStop = "Кнопка \"Стоп\""
TitleFiles = "Ассоциации файлов"
TitleOpen = "Открыть аудиофайлы"
TitleSave = "Сохранить ассоциации"
TitleSkipErr = "Пропустить ошибки"
TitleShowPath = "Показать путь"
TitleShowExt = "Показать расширение"
TitleDelete = "Удалить файл из списка"
MsgPlaylistEmpty = "Плейлист пуст. Наберите в этой строке перечень идентификаторов в том порядке, в котором соответствующие им файлы должны воспроизводиться."
MsgPlaylistBadkey = "Ошибка в плейлисте! Ошибка возникла при воспроизведении идентификатора \"%s\" в позиции %i."
MsgPlaylistHelp = "Плейлист воспроизведения. Здесь идентификаторы перечисляются через пробел."
MsgPlayHelp = "Воспроизвести файлы из плейлиста, либо приостановить воспроизведение."
MsgStopHelp = "Остановить воспроизведение файлов из плейлиста."
MsgFilesPlay = "Идентификатор файла не может быть изменён во время воспроизведения."
MsgFilesHelp = "Ассоциации аудиофайлов с идентификаторами. В правой колонке перечислены файлы, в левой - соответствующие им идентификаторы. Эти идентификаторы затем используются в плейлисте."
MsgOpenHelp = "Найти и выбрать аудиофайлы, которым можно сопоставлять идентификаторы. Можно выделить и выбрать сразу несколько файлов. Кроме того, можно нужные файлы выделить в проводнике, и перетащить мышью на окно программы."
MsgSaveHelp = "Сохранить установленные ассоциации в файл. Этот файл будет использоваться плеером при воспроизведении плейлиста, а также загружаться данным редактором."
MsgSkipErrHelp = "Если кнопка вдавлена, то ошибки в плейлисте будут игнорироваться. Иначе воспроизведение будет остановлено."
MsgShowPathHelp = "Если кнопка вдавлена, то в списке будет отображаться полный путь к файлу. Иначе будет отображаться только имя файла."
MsgShowExtHelp = "Если кнопка вдавлена, то будут отображаться расширения файлов."
MsgDeleteHelp = "Нажатием на эту кнопку будет удалён выделенный файл из списка."

-- Таблица задаёт разбиение списка ассоциаций по колонкам
PlaylistColumns = {
	{"Идентификатор", 100},
	{"Файл", 360},
}

-- Список фильтров расширений звуковых файлов для диалога открытия
ExtFilter = {
	{"All audio files", "*.wav;*.mp3;*.wma;*.ogg;*.aif;*.aiff"},
	{"Plain wave audio (*.wav)", "*.wav"},
	{"MPEG Layer-3 (*.mp3)", "*.mp3"},
	{"Ogg Vorbis (*.ogg)", "*.ogg"},
	{"Windows Media Audio (*.wma)", "*.wma"},
	{"Audio Interchange File Format (*.aif)", "*.aif;*.aiff"},
}

-- Расширение файлов с плейлистом
ExtPlaylist = "txt"

-- Список допустимых расширений файлов.
-- Файлы с расширениями, не входящими в список - включаться не будут.
ExtAllowed = {
	"wav", "mp3", "wma", "ogg", "aif", "aiff",
}

-------------------------------------------------------------------------------
-- Helper functions
-------------------------------------------------------------------------------

-- Экранирование обратных слэшей при записи строк в скрипт Lua
function escapeLua(str)
	local res = ""
	local i, j = 1
	repeat
		j = string.find(str, "\\", i)
		res = res..string.sub(str, i, j and j-1)..(j and "\\\\" or "")
		i = j and j+1
	until not i
	return res
end

-- Сохранение таблицы ассоциаций KeyFiles в файл скрипта.
function saveScript()
	local file = io.open("keyfiles.lua", "w")
	if file then
		file:write("\239\187\191".. -- UTF-8 BOM
[[---UTF8------------------------------------------------------------------------
--                   © BEOWOLF / Podobashev Dmitry, 2011                     --
--     email: xbeowolf@gmail.com     skype: x-beowulf     icq: 320329575     --
-------------------------------------------------------------------------------

-- Скрипт сгенерирован программой Seismix editor.
-- Редактировать этот скрипт можно в Notepad++ http://notepad-plus-plus.org/

-- Таблица ассоциаций ключей плейлиста с музыкальными файлами.
-- Данная таблица - упрощённый KeyFuncs, соответствующий ключу файл
-- проигрывается синхронно (без микширования), ключу нельзя сопоставить
-- несколько действий.
KeyFiles = {
]])
		for k, v in pairs(KeyFiles) do
			file:write("\t[\""..escapeLua(k).."\"] = \""..escapeLua(v).."\",\n")
		end
		file:write(
[[
}

-- The End.]])
		io.close(file)
	end
end

-------------------------------------------------------------------------------
-- Windows messages
-------------------------------------------------------------------------------

-- WM_DESTROY
function JSeismix:wmDestroy()
	wnd.BaloonHide()
end

-- WM_CLOSE
function JSeismix:wmClose()
	self:wmCommand(idcCancel, 0)
end

-- WM_ACTIVATEAPP
function JSeismix:wmActivateApp(activated, idThread)
	if activated then
		wnd.setFocus(idcPlaylist)
	else
		wnd.BaloonHide()
	end
end

-- WM_COMMAND preprocess call
function JSeismix:wmCommand(cmd, ctrl)
	local result = true
	wnd.BaloonHide()
	if cmd == idcSave then
		saveScript()
		wnd.BaloonShowCtrl(idcSave, "Данные успешно сохранены", "Сохранение", 1, 0x000000)
	elseif cmd == idcCancel then
		wnd.WindowDestroy()
	else
		result = false
	end
	return result
end

function JSeismix:wmHelp(ctrl, x, y)
	local message, title
	if ctrl == idcPlaylist then
		title, message = TitlePlaylist, MsgPlaylistHelp
	elseif ctrl == idcPlay then
		title, message = TitlePlay, MsgPlayHelp
	elseif ctrl == idcStop then
		title, message = TitleStop, MsgStopHelp
	elseif ctrl == idcFiles then
		title, message = TitleFiles, MsgFilesHelp
	elseif ctrl == idcOpen then
		title, message = TitleOpen, MsgOpenHelp
	elseif ctrl == idcSave then
		title, message = TitleSave, MsgSaveHelp
	elseif ctrl == idcSkipErr then
		title, message = TitleSkipErr, MsgSkipErrHelp
	elseif ctrl == idcShowPath then
		title, message = TitleShowPath, MsgShowPathHelp
	elseif ctrl == idcShowExt then
		title, message = TitleShowExt, MsgShowExtHelp
	elseif ctrl == idcDelete then
		title, message = TitleDelete, MsgDeleteHelp
	end

	if message then
		if not title then title = "Подсказка:" end
		wnd.BaloonShowPoint(x, y, message, title, 1, 0x000000)
	end
end

-- The End.