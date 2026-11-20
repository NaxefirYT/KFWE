# Переводы KFWE / KFWE translations

Язык выбирается в окне «Конфиг» на вкладке «Язык». Каждый язык это папка в `share\locale`, например `en-US`. Чтобы язык появился в списке, в папке достаточно файла `kfwe.txt`.

Как добавить свой язык:

1. Создайте папку `share\locale\<код>`, например `de-DE`.
2. Скопируйте в неё `en-US\kfwe.txt`.
3. В первой строке напишите название языка на нём самом: `@name = Deutsch`.
4. В каждой строке слева русский оригинал, справа после ` = ` перевод. Меняйте только правую часть.
5. Запустите KFWE, выберите язык в «Конфиг» → «Язык» и перезапустите редактор.

Окно настроек и меню KFWE переводятся из этого файла сами. Тексты самого редактора (триггеры, редактор объектов) лежат в `share\<код>\mpq`. Если этой папки нет, они будут на русском.

Правила:
- `\n` это перенос строки, `\t` табуляция, `\s` пробел в начале или в конце строки. Их нужно сохранить.
- `%d`, `%s`, `%ls`, `%hs`, `{0}`, `{1:F2}` подставляют числа и имена. Оставьте их в том же порядке.
- Если строки нет или правая часть пустая, будет показан русский текст.
- Строки, начинающиеся с `#`, это комментарии.

Готовый файл присылайте в Discord или пул-реквестом на GitHub.

---

The language is chosen in the «Config» window, on the «Language» tab. Each language is a folder in `share\locale`, for example `en-US`. A `kfwe.txt` file in the folder is enough for the language to show up in the list.

To add your language:

1. Create a folder `share\locale\<code>`, for example `de-DE`.
2. Copy `en-US\kfwe.txt` into it.
3. Put the language name in that language on the first line: `@name = Deutsch`.
4. Every line has the Russian original on the left and the translation after ` = ` on the right. Change only the right side.
5. Start KFWE, pick the language in «Config» → «Language» and restart the editor.

The settings window and the KFWE menu are translated from this file automatically. The editor's own texts (triggers, Object Editor) live in `share\<code>\mpq`. Without that folder they stay Russian.

Rules:
- `\n` is a line break, `\t` a tab, `\s` a space at the start or end of a line. Keep them.
- `%d`, `%s`, `%ls`, `%hs`, `{0}`, `{1:F2}` insert numbers and names. Keep them in the same order.
- A missing line or an empty right side shows the Russian text.
- Lines starting with `#` are comments.

Send the finished file on Discord or as a pull request on GitHub.
