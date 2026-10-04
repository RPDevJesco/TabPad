T = native
B = build/$(T)
HOSTCC = cc

ifeq ($(T),win64)
CC = x86_64-w64-mingw32-gcc
AR = x86_64-w64-mingw32-ar
NM = x86_64-w64-mingw32-nm
EXE = .exe
RUN = wine
STORE = port/win32/store_win32.c
SO = .dll
DESK = port/win32/desk_win32.c
WINDRES = x86_64-w64-mingw32-windres
RES = $(B)/tabpad_res.o
SDL_CFLAGS = -I$(SDL3_DIR)/include
SDL_LIBS = -L$(SDL3_DIR)/lib -lSDL3 -mwindows
SDL_DLL = $(B)/SDL3.dll
else
CC = cc
AR = ar
NM = nm
EXE =
RUN =
STORE = port/posix/store_posix.c
SO = $(if $(filter Darwin,$(shell uname)),.dylib,.so)
DESK = port/posix/desk_posix.c
RES =
SDL_STATIC =
SDL_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL_LIBS = $(shell pkg-config --libs $(SDL_STATIC) sdl3) -lm
SDL_DLL =
endif

TS = vendor/tree-sitter/lib
FREE = -ffreestanding -fno-builtin -fno-stack-protector
WARN = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes
HL_CFLAGS = -std=c99 -O2 $(WARN) $(FREE) -Ishim -Iinclude -I$(TS)/include
SESS_CFLAGS = -std=c99 -O2 $(WARN) $(FREE) -Iinclude
ED_CFLAGS = -std=c99 -O2 $(WARN) $(FREE) -Iinclude
VENDOR_CFLAGS = -std=c11 -O2 -w $(FREE) -DNDEBUG -Ishim -I$(TS)/include -I$(TS)/src
HOSTED_CFLAGS = -std=c99 -O2 $(WARN) -Iinclude -Isrc -Iport -Iport/hosted
HL_OWN = $(B)/hl.o $(B)/hl_text.o $(B)/hl_re.o $(B)/hl_query.o $(B)/hl_libc.o
HL = $(HL_OWN) $(B)/ts.o
SESS = $(B)/sess.o $(B)/sess_crc.o
ED = $(B)/ed.o $(B)/ed_tab.o $(B)/ed_edit.o $(B)/ed_view.o $(B)/ed_file.o $(B)/ed_undo.o \
	$(B)/ed_cmd.o $(B)/ed_menu.o $(B)/ed_tool.o $(B)/ed_find.o $(B)/ed_status.o \
	$(B)/ed_word.o $(B)/ed_cfg.o $(B)/ed_list.o
LANG_NAMES = c cpp csharp css javascript html lua apex python ruby rust markdown markdowninline json xml bash powershell
SCAN_NAMES = cpp csharp css javascript html lua python ruby rust markdown markdowninline xml bash powershell
LANG_OBJS = $(LANG_NAMES:%=$(B)/lang_%.o) $(SCAN_NAMES:%=$(B)/scan_%.o) $(LANG_NAMES:%=$(B)/query_%.o)
LANGS = $(B)/liblang.a
PORT = $(B)/port_hosted.o
BINS = $(B)/test_hl$(EXE) $(B)/test_langs$(EXE) $(B)/test_sess$(EXE) $(B)/test_ed$(EXE) $(B)/hlcat$(EXE)

all: $(B)/libhl.a $(B)/libsess.a $(B)/libed.a $(BINS)

test: all
	sh tools/check_freestanding.sh $(NM) hl $(B)/libhl.a $(LANGS)
	sh tools/check_freestanding.sh $(NM) sess $(B)/libsess.a
	sh tools/check_freestanding.sh $(NM) "(ed|hl|sess)" $(B)/libed.a
	$(RUN) $(B)/test_hl$(EXE)
	$(RUN) $(B)/test_langs$(EXE) test/samples
	$(RUN) $(B)/test_sess$(EXE) $(B)/sess_test.tmp
	$(RUN) $(B)/test_ed$(EXE) $(B)/ed_test.tmp work
	$(RUN) $(B)/test_ed$(EXE) $(B)/ed_test.tmp back

clean:
	rm -rf build

$(B):
	mkdir -p $(B)

$(HL_OWN): $(B)/%.o: src/%.c include/hl.h src/hl_int.h shim/hl_c.h | $(B)
	$(CC) $(HL_CFLAGS) -c $< -o $@

$(B)/ts.o: $(TS)/src/lib.c | $(B)
	$(CC) $(VENDOR_CFLAGS) -c $< -o $@

$(B)/libhl.a: $(HL)
	rm -f $@
	$(AR) rcs $@ $(HL)

$(SESS): $(B)/%.o: src/%.c include/sess.h include/sess_format.h | $(B)
	$(CC) $(SESS_CFLAGS) -c $< -o $@

$(B)/libsess.a: $(SESS)
	rm -f $@
	$(AR) rcs $@ $(SESS)

$(ED): $(B)/%.o: src/%.c include/ed.h include/ed_words.h include/hl.h include/sess.h include/sess_format.h src/ed_int.h | $(B)
	$(CC) $(ED_CFLAGS) -c $< -o $@

$(B)/libed.a: $(ED)
	rm -f $@
	$(AR) rcs $@ $(ED)

$(B)/embed: tools/embed.c | $(B)
	$(HOSTCC) -std=c99 -O2 $(WARN) $< -o $@

$(LANG_NAMES:%=$(B)/lang_%.o): $(B)/lang_%.o: vendor/tree-sitter-%/src/parser.c | $(B)
	$(CC) $(VENDOR_CFLAGS) -c $< -o $@

$(SCAN_NAMES:%=$(B)/scan_%.o): $(B)/scan_%.o: vendor/tree-sitter-%/src/scanner.c | $(B)
	$(CC) $(VENDOR_CFLAGS) -Ivendor/tree-sitter-$*/src -c $< -o $@

V = vendor/tree-sitter
Q_cpp = $(V)-c/queries/highlights.scm $(V)-cpp/queries/highlights.scm
Q_javascript = $(V)-javascript/queries/highlights.scm $(V)-javascript/queries/highlights-jsx.scm \
	$(V)-javascript/queries/highlights-params.scm lang/javascript.scm
Q_apex = $(V)-apex/queries/highlights.scm $(V)-apex/queries/highlights-sosl.scm $(V)-apex/queries/highlights-soql.scm
Q_json = $(V)-json/queries/highlights.scm lang/json.scm
Q_lua = $(V)-lua/queries/highlights.scm lang/lua.scm
Q_python = $(V)-python/queries/highlights.scm lang/python.scm
Q_powershell = $(V)-powershell/queries/highlights.scm lang/powershell.scm
query_of = $(or $(Q_$(1)),vendor/tree-sitter-$(1)/queries/highlights.scm)

.SECONDEXPANSION:
$(LANG_NAMES:%=$(B)/query_%.c): $(B)/query_%.c: $$(call query_of,$$*) $(B)/embed
	$(B)/embed hl_query_$* $@ $(call query_of,$*)

$(LANG_NAMES:%=$(B)/query_%.o): $(B)/query_%.o: $(B)/query_%.c
	$(CC) $(HL_CFLAGS) -c $< -o $@

$(B)/liblang.a: $(LANG_OBJS)
	rm -f $@
	$(AR) rcs $@ $(LANG_OBJS)

$(B)/langs.o: lang/langs.c lang/langs.h include/hl.h include/ed.h | $(B)
	$(CC) $(HL_CFLAGS) -Ilang -c $< -o $@

$(B)/theme.o: port/theme_dark.c include/ed.h include/hl.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

$(B)/icons.o: port/icons.c include/ed.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

$(B)/port_hosted.o: port/hosted/port_hosted.c port/hosted/port_hosted.h include/ed.h include/hl.h include/sess.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

$(B)/store.o: $(STORE) port/store.h include/sess.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

$(B)/test_hl$(EXE): test/test_hl.c $(PORT) $(LANGS) $(B)/libhl.a
	$(CC) $(HOSTED_CFLAGS) $< $(PORT) $(LANGS) $(B)/libhl.a -o $@

$(B)/test_langs$(EXE): test/test_langs.c $(B)/langs.o $(B)/theme.o $(PORT) $(LANGS) $(B)/libhl.a
	$(CC) $(HOSTED_CFLAGS) $< $(B)/langs.o $(B)/theme.o $(PORT) $(LANGS) $(B)/libhl.a -o $@

$(B)/test_sess$(EXE): test/test_sess.c $(PORT) $(B)/store.o $(B)/libsess.a
	$(CC) $(HOSTED_CFLAGS) $< $(PORT) $(B)/store.o $(B)/libsess.a -o $@

EDITOR = $(B)/libed.a $(B)/libsess.a $(B)/langs.o $(B)/theme.o $(B)/icons.o $(PORT) $(B)/store.o $(LANGS) $(B)/libhl.a

$(B)/test_ed$(EXE): test/test_ed.c $(EDITOR)
	$(CC) $(HOSTED_CFLAGS) $< $(EDITOR) -o $@

$(B)/hlcat$(EXE): tools/hlcat.c $(B)/langs.o $(PORT) $(LANGS) $(B)/libhl.a
	$(CC) $(HOSTED_CFLAGS) $< $(B)/langs.o $(PORT) $(LANGS) $(B)/libhl.a -o $@

SDL3 = $(B)/main_sdl3.o $(B)/draw_sdl3.o $(B)/plug_sdl3.o $(B)/tongue_sdl3.o $(B)/stb_sdl3.o $(B)/desk.o \
	$(B)/font_data.o $(B)/tongue_zh_cn.o $(B)/tongue_template.o $(RES)

app: $(B)/tabpad$(EXE) $(SDL_DLL)

$(B)/SDL3.dll: $(SDL3_DIR)/bin/SDL3.dll | $(B)
	cp $< $@

PLUG = $(B)/plug.tmp

plugin: | $(B)
	rm -rf $(PLUG)
	mkdir -p $(PLUG)/jsonc $(PLUG)/nothing
	$(CC) -shared -fPIC -O2 -w vendor/tree-sitter-json/src/parser.c -o $(PLUG)/jsonc/jsonc$(SO)
	cp vendor/tree-sitter-json/queries/highlights.scm $(PLUG)/jsonc/
	printf 'title = JSON with Comments\nname = JSON with comments file\nendings = .jsonc .json5\nsymbol = json\n' > $(PLUG)/jsonc/language.ini
	printf '{ "a": [1, true] } // kept\n' > $(PLUG)/sample.jsonc

smoke: app plugin
	rm -rf $(B)/smoke.tmp $(B)/smoke_whole.tmp $(B)/smoke_plug.tmp
	SDL_VIDEODRIVER=dummy $(RUN) $(B)/tabpad$(EXE) --session $(B)/smoke.tmp --type "typed by make smoke, " --shot $(B)/smoke_first.bmp
	SDL_VIDEODRIVER=dummy $(RUN) $(B)/tabpad$(EXE) --session $(B)/smoke.tmp --type "never saved" --shot $(B)/smoke_restored.bmp
	SDL_VIDEODRIVER=dummy $(RUN) $(B)/tabpad$(EXE) --session $(B)/smoke_whole.tmp --type "typed by make smoke, never saved" --shot $(B)/smoke_whole.bmp
	cmp $(B)/smoke_restored.bmp $(B)/smoke_whole.bmp
	@echo "smoke: the restarted editor went on from what was typed"
	SDL_VIDEODRIVER=dummy $(RUN) $(B)/tabpad$(EXE) --session $(B)/smoke_plug.tmp --languages $(PLUG) --shot $(B)/smoke_plug.bmp $(PLUG)/sample.jsonc
	grep -q "^jsonc: JSON with Comments: added, 2 endings of its own" $(PLUG)/languages.log
	grep -q "^nothing: left out" $(PLUG)/languages.log
	@echo "smoke: a language plugin loaded, and a folder that is not one was left out"

$(B)/desk.o: $(DESK) port/desk.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -c $< -o $@

$(B)/tabpad_res.o: port/win32/tabpad.rc port/win32/tabpad.ico | $(B)
	$(WINDRES) -I port/win32 $< -O coff -o $@

$(B)/font_data.c: vendor/dejavu/DejaVuSansMono.ttf $(B)/embed
	$(B)/embed -b sdl3_font_data $@ vendor/dejavu/DejaVuSansMono.ttf

$(B)/tongue_zh_cn.c: translations/zh-CN.txt $(B)/embed
	$(B)/embed -b sdl3_tongue_zh_cn $@ translations/zh-CN.txt

$(B)/tongue_template.c: translations/english.template $(B)/embed
	$(B)/embed -b sdl3_tongue_template $@ translations/english.template

$(B)/font_data.o $(B)/tongue_zh_cn.o $(B)/tongue_template.o: $(B)/%.o: $(B)/%.c
	$(CC) -O2 -c $< -o $@

$(B)/main_sdl3.o $(B)/draw_sdl3.o $(B)/plug_sdl3.o $(B)/tongue_sdl3.o: $(B)/%.o: port/sdl3/%.c port/sdl3/sdl3.h include/ed.h include/ed_words.h include/hl.h lang/langs.h port/store.h port/desk.h | $(B)
	$(CC) $(HOSTED_CFLAGS) -Wno-missing-prototypes $(SDL_CFLAGS) -Iport/sdl3 -Ilang -Ivendor/stb -c $< -o $@

$(B)/stb_sdl3.o: port/sdl3/stb_sdl3.c vendor/stb/stb_truetype.h | $(B)
	$(CC) -O2 -w -Ivendor/stb -c $< -o $@

$(B)/tabpad$(EXE): $(SDL3) $(EDITOR)
	$(CC) $(SDL3) $(EDITOR) $(SDL_LIBS) -o $@

WEB = build/web
WEB_CC = clang --target=wasm32 -mbulk-memory -std=c99 -O2 $(FREE) -fvisibility=hidden
WEB_VENDOR_CC = clang --target=wasm32 -mbulk-memory -std=c11 -O2 -w $(FREE) -fvisibility=hidden -DNDEBUG -Ishim -I$(TS)/include -I$(TS)/src
WEB_OWN = hl hl_text hl_re hl_query hl_libc sess sess_crc ed ed_tab ed_edit ed_view ed_file ed_undo ed_cmd ed_menu ed_tool \
	ed_find ed_status ed_word ed_cfg ed_list
WEB_OBJS = $(WEB_OWN:%=$(WEB)/%.o) $(WEB)/ts.o $(WEB)/langs.o $(WEB)/theme.o $(WEB)/icons.o $(WEB)/port_web.o $(WEB)/tongue_zh_cn.o \
	$(LANG_NAMES:%=$(WEB)/lang_%.o) $(SCAN_NAMES:%=$(WEB)/scan_%.o) $(LANG_NAMES:%=$(WEB)/query_%.o)
WEB_EXPORTS = ed_init ed_resize ed_key ed_text ed_mouse_down ed_mouse_move ed_mouse_up ed_wheel ed_tick ed_focus_lost \
	ed_open ed_picked ed_stale ed_draw ed_key_cmd web_alloc web_free

web: $(WEB)/tabpad.html

$(WEB):
	mkdir -p $(WEB)

$(WEB)/embed: tools/embed.c | $(WEB)
	$(HOSTCC) -std=c99 -O2 $< -o $@

$(WEB_OWN:%=$(WEB)/%.o): $(WEB)/%.o: src/%.c include/ed.h include/ed_words.h include/hl.h include/sess.h include/sess_format.h src/ed_int.h src/hl_int.h | $(WEB)
	$(WEB_CC) $(WARN) -Ishim -Iinclude -I$(TS)/include -c $< -o $@

$(WEB)/ts.o: $(TS)/src/lib.c | $(WEB)
	$(WEB_VENDOR_CC) -c $< -o $@

$(LANG_NAMES:%=$(WEB)/lang_%.o): $(WEB)/lang_%.o: vendor/tree-sitter-%/src/parser.c | $(WEB)
	$(WEB_VENDOR_CC) -c $< -o $@

$(SCAN_NAMES:%=$(WEB)/scan_%.o): $(WEB)/scan_%.o: vendor/tree-sitter-%/src/scanner.c | $(WEB)
	$(WEB_VENDOR_CC) -Ivendor/tree-sitter-$*/src -c $< -o $@

$(LANG_NAMES:%=$(WEB)/query_%.c): $(WEB)/query_%.c: $$(call query_of,$$*) $(WEB)/embed
	$(WEB)/embed hl_query_$* $@ $(call query_of,$*)

$(WEB)/tongue_zh_cn.c: translations/zh-CN.txt $(WEB)/embed
	$(WEB)/embed -b web_tongue_zh_cn $@ translations/zh-CN.txt

$(LANG_NAMES:%=$(WEB)/query_%.o) $(WEB)/tongue_zh_cn.o: $(WEB)/%.o: $(WEB)/%.c
	$(WEB_CC) -c $< -o $@

$(WEB)/langs.o: lang/langs.c lang/langs.h include/hl.h include/ed.h | $(WEB)
	$(WEB_CC) $(WARN) -Iinclude -Ilang -c $< -o $@

$(WEB)/theme.o: port/theme_dark.c include/ed.h include/hl.h | $(WEB)
	$(WEB_CC) $(WARN) -Iinclude -c $< -o $@

$(WEB)/icons.o: port/icons.c include/ed.h | $(WEB)
	$(WEB_CC) $(WARN) -Iinclude -c $< -o $@

$(WEB)/port_web.o: port/web/port_web.c include/ed.h include/hl.h include/sess.h | $(WEB)
	$(WEB_CC) $(WARN) -Iinclude -c $< -o $@

$(WEB)/tabpad.wasm: $(WEB_OBJS) Makefile
	wasm-ld --no-entry --strip-all --import-undefined -z stack-size=1048576 $(WEB_EXPORTS:%=--export=%) $(WEB_OBJS) -o $@

$(WEB)/tabpad.html: $(WEB)/tabpad.wasm port/web/page.html port/web/page.css port/web/tabpad.js tools/pack_web.sh
	sh tools/pack_web.sh $(WEB)/tabpad.wasm port/web/page.html port/web/page.css port/web/tabpad.js $@

web-smoke: $(WEB)/tabpad.wasm
	node tools/web_smoke.mjs $(WEB)/tabpad.wasm

.PHONY: all test app plugin smoke clean web web-smoke
