'use strict';

(async function ()
{
    const BASE_PX = 13;
    const TICK_MS = 200;
    const KEEP_MS = 400;
    const WHEEL_STEP = 100;
    const DB_NAME = 'tabpad';
    const LAST_WORDS = 'tabpad:';
    const STORES = ['session', 'files', 'handles'];
    const KEYS = {
        ArrowLeft: 1, ArrowRight: 2, ArrowUp: 3, ArrowDown: 4, Home: 5, End: 6, PageUp: 7, PageDown: 8,
        Backspace: 9, Delete: 10, Enter: 11, Tab: 12, Escape: 13, Insert: 14, F3: 15,
    };
    const SHIFT = 1;
    const CTRL = 2;
    const ALT = 4;
    const MENU_LETTERS = 'fesvnlw';
    const STACKS = [
        'system-ui, "Segoe UI", "Helvetica Neue", sans-serif',
        'Consolas, "Cascadia Mono", Menlo, "DejaVu Sans Mono", "Liberation Mono", monospace',
    ];
    const KNOWN_FONTS = [
        'Arial', 'Avenir', 'Calibri', 'Cambria', 'Candara', 'Cantarell', 'Cascadia Code', 'Cascadia Mono', 'Consolas', 'Courier', 'Courier New',
        'DejaVu Sans', 'DejaVu Sans Mono', 'DejaVu Serif', 'Fira Code', 'Fira Mono', 'Georgia', 'Hack', 'Helvetica', 'Helvetica Neue',
        'IBM Plex Mono', 'Inconsolata', 'Inter', 'JetBrains Mono', 'Liberation Mono', 'Liberation Sans', 'Liberation Serif', 'Lucida Console',
        'Menlo', 'Microsoft YaHei', 'Monaco', 'MS Gothic', 'Noto Sans', 'Noto Sans CJK SC', 'Noto Sans Mono', 'Noto Serif', 'PingFang SC',
        'Roboto', 'Roboto Mono', 'Segoe UI', 'SF Mono', 'SimSun', 'Source Code Pro', 'Tahoma', 'Times New Roman', 'Trebuchet MS',
        'Ubuntu', 'Ubuntu Mono', 'Verdana', 'WenQuanYi Micro Hei', 'Yu Gothic',
    ];

    const canvas = document.getElementById('c');
    const input = document.getElementById('t');
    const ctx = canvas.getContext('2d', { alpha: false });
    const probe = document.createElement('canvas').getContext('2d');
    const encoder = new TextEncoder();
    const decoder = new TextDecoder();
    const blobs = new Map();
    const files = new Map();
    const handles = new Map();
    const looks = new Map();
    const colours = new Map();
    const chosen = ['', ''];
    const unsettled = new Map();

    let db = null;
    let api = null;
    let memory = null;
    let stopped = false;
    let putting = null;
    let getting = null;
    let got_at = 0;
    let listing = [];
    let listed = 0;
    let clip = '';
    let clip_at = 0;
    let in_clip_event = false;
    let look = null;
    let clipped = false;
    let fonts = null;
    let font_names = [];
    let title = 'new';
    let frame_due = false;
    let wheel_rest = 0;
    let scratch = 0;
    let keep_timer = 0;

    function fail(text)
    {
        const p = document.createElement('p');

        p.textContent = text;
        document.body.replaceChildren(p);
    }

    function bytes()
    {
        return new Uint8Array(memory.buffer);
    }

    function view()
    {
        return new DataView(memory.buffer);
    }

    function text_at(at)
    {
        const all = bytes();
        let end = at;

        while (all[end])
        {
            end++;
        }

        return decoder.decode(all.subarray(at, end));
    }

    function units_at(at, n)
    {
        return new Uint16Array(memory.buffer, at, n);
    }

    function give(text)
    {
        const raw = encoder.encode(text);
        const at = api.web_alloc(raw.length + 1);

        if (!at)
        {
            return 0;
        }

        bytes().set(raw, at);
        bytes()[at + raw.length] = 0;

        return at;
    }

    function with_text(text, call)
    {
        const at = give(text);
        const result = at ? call(at) : -1;

        api.web_free(at);

        return result;
    }

    function open_db()
    {
        return new Promise(function (done)
        {
            let request;

            try
            {
                request = indexedDB.open(DB_NAME, 1);
            }

            catch (e)
            {
                done(null);
                return;
            }

            request.onupgradeneeded = function ()
            {
                for (const name of STORES)
                {
                    request.result.createObjectStore(name);
                }
            };

            request.onsuccess = function ()
            {
                done(request.result);
            };

            request.onerror = function ()
            {
                done(null);
            };

            request.onblocked = function ()
            {
                done(null);
            };
        });
    }

    function read_store(name, into)
    {
        return new Promise(function (done)
        {
            const cursor = db.transaction(name).objectStore(name).openCursor();

            cursor.onsuccess = function ()
            {
                if (!cursor.result)
                {
                    done();
                    return;
                }

                into.set(cursor.result.key, cursor.result.value);
                cursor.result.continue();
            };

            cursor.onerror = function ()
            {
                done();
            };
        });
    }

    function keep(store, key, value)
    {
        let mark = null;

        if (!db)
        {
            return;
        }

        if (store === 'session')
        {
            mark = {};
            unsettled.set(key, mark);
        }

        try
        {
            const t = db.transaction(store, 'readwrite');
            const s = t.objectStore(store);

            if (value === undefined)
            {
                s.delete(key);
            }

            else
            {
                s.put(value, key);
            }

            t.oncomplete = function ()
            {
                if (unsettled.get(key) === mark)
                {
                    unsettled.delete(key);
                }
            };
        }

        catch (e)
        {
            db = null;
        }
    }

    function last_words()
    {
        try
        {
            for (const key of unsettled.keys())
            {
                const raw = blobs.get(key);
                let text = raw ? 'p' : 'd';

                for (let at = 0; raw && at < raw.length; at += 8192)
                {
                    text += String.fromCharCode.apply(null, raw.subarray(at, at + 8192));
                }

                localStorage.setItem(LAST_WORDS + key, text);
            }
        }

        catch (e)
        {
        }
    }

    function take_last_words()
    {
        const found = [];

        try
        {
            for (let i = 0; i < localStorage.length; i++)
            {
                if (localStorage.key(i).startsWith(LAST_WORDS))
                {
                    found.push(localStorage.key(i));
                }
            }

            for (const name of found)
            {
                const text = localStorage.getItem(name);
                const key = name.slice(LAST_WORDS.length);

                if (text[0] === 'p')
                {
                    const raw = new Uint8Array(text.length - 1);

                    for (let at = 1; at < text.length; at++)
                    {
                        raw[at - 1] = text.charCodeAt(at);
                    }

                    blobs.set(key, raw);
                    keep('session', key, raw);
                }

                else
                {
                    blobs.delete(key);
                    keep('session', key, undefined);
                }

                localStorage.removeItem(name);
            }
        }

        catch (e)
        {
        }
    }

    function forget_last_words()
    {
        try
        {
            for (let i = localStorage.length; i > 0; i--)
            {
                if (localStorage.key(i - 1).startsWith(LAST_WORDS))
                {
                    localStorage.removeItem(localStorage.key(i - 1));
                }
            }
        }

        catch (e)
        {
        }
    }

    function rgb(v)
    {
        let s = colours.get(v);

        if (!s)
        {
            s = '#' + (v & 0xFFFFFF).toString(16).padStart(6, '0');
            colours.set(v, s);
        }

        return s;
    }

    function family(which)
    {
        return (chosen[which] ? '"' + chosen[which] + '", ' : '') + STACKS[which];
    }

    function installed(name)
    {
        const sample = 'mmmmmmmmmmlliWQ@0123';
        const bases = ['monospace', 'serif'];

        for (const base of bases)
        {
            probe.font = '72px ' + base;

            const plain = probe.measureText(sample).width;

            probe.font = '72px "' + name + '", ' + base;

            if (probe.measureText(sample).width !== plain)
            {
                return true;
            }
        }

        return false;
    }

    function find_fonts()
    {
        if (!fonts)
        {
            fonts = KNOWN_FONTS.filter(installed);
            font_names = fonts.map(give);
        }

        return fonts;
    }

    function width_of(cp)
    {
        let w = look.widths.get(cp);

        if (w === undefined)
        {
            w = Math.round(ctx.measureText(String.fromCodePoint(cp)).width);
            look.widths.set(cp, w);
        }

        return w;
    }

    function next_cp(units, i)
    {
        const hi = units[i];

        if (hi >= 0xD800 && hi <= 0xDBFF && i + 1 < units.length && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF)
        {
            return 0x10000 + ((hi - 0xD800) << 10) + (units[i + 1] - 0xDC00);
        }

        return hi >= 0xD800 && hi <= 0xDFFF ? 0xFFFD : hi;
    }

    function size()
    {
        const dpr = window.devicePixelRatio || 1;
        const w = Math.max(1, Math.round(canvas.clientWidth * dpr));
        const h = Math.max(1, Math.round(canvas.clientHeight * dpr));

        if (canvas.width !== w || canvas.height !== h)
        {
            canvas.width = w;
            canvas.height = h;
            clipped = false;
        }

        return [w, h, Math.round(dpr * 100)];
    }

    function paint()
    {
        frame_due = false;

        if (!stopped && api.ed_stale())
        {
            api.ed_draw();
        }
    }

    function leave()
    {
        clearTimeout(keep_timer);
        keep_timer = 0;

        if (!stopped)
        {
            api.ed_focus_lost();
        }
    }

    function going()
    {
        leave();
        last_words();
    }

    function kick()
    {
        if (!frame_due && !stopped)
        {
            frame_due = true;
            requestAnimationFrame(paint);
        }
    }

    function touched()
    {
        clearTimeout(keep_timer);
        keep_timer = setTimeout(leave, KEEP_MS);
        kick();
    }

    function mods_of(e)
    {
        return (e.shiftKey ? SHIFT : 0) | (e.ctrlKey || e.metaKey ? CTRL : 0) | (e.altKey ? ALT : 0);
    }

    function point(e)
    {
        const box = canvas.getBoundingClientRect();
        const sx = canvas.width / box.width;
        const sy = canvas.height / box.height;

        return [Math.floor((e.clientX - box.left) * sx), Math.floor((e.clientY - box.top) * sy)];
    }

    function leaf(path)
    {
        return path.slice(path.lastIndexOf('/') + 1);
    }

    function hold(name, raw, handle)
    {
        const path = '/' + name;
        const file = { bytes: raw, mtime: Date.now() };

        files.set(path, file);
        keep('files', path, file);

        if (handle)
        {
            handles.set(path, handle);
            keep('handles', path, handle);
        }

        return path;
    }

    function download(name, raw)
    {
        const a = document.createElement('a');
        const url = URL.createObjectURL(new Blob([raw], { type: 'application/octet-stream' }));

        a.href = url;
        a.download = name;
        a.click();
        setTimeout(function ()
        {
            URL.revokeObjectURL(url);
        }, 60000);
    }

    async function write_through(handle, raw)
    {
        const mode = { mode: 'readwrite' };
        let state = await handle.queryPermission(mode);

        if (state !== 'granted')
        {
            state = await handle.requestPermission(mode);
        }

        if (state !== 'granted')
        {
            throw new Error('refused');
        }

        const out = await handle.createWritable();

        await out.write(raw);
        await out.close();
    }

    function picked(what, path)
    {
        if (path === null)
        {
            api.ed_picked(what, 0);
        }

        else
        {
            with_text(path, function (at)
            {
                api.ed_picked(what, at);

                return 0;
            });
        }

        touched();
    }

    async function take(file, handle)
    {
        return hold(file.name, new Uint8Array(await file.arrayBuffer()), handle);
    }

    function pick_by_input()
    {
        const chooser = document.createElement('input');

        chooser.type = 'file';
        chooser.multiple = true;
        chooser.onchange = async function ()
        {
            for (const file of chooser.files)
            {
                picked(0, await take(file, null));
            }
        };
        chooser.click();
    }

    async function pick_open()
    {
        let chosen_handles;

        if (!window.showOpenFilePicker)
        {
            pick_by_input();
            return;
        }

        try
        {
            chosen_handles = await window.showOpenFilePicker({ multiple: true });
        }

        catch (e)
        {
            if (e.name !== 'AbortError')
            {
                pick_by_input();
            }

            return;
        }

        for (const handle of chosen_handles)
        {
            picked(0, await take(await handle.getFile(), handle));
        }
    }

    async function pick_save()
    {
        let handle = null;
        let name = null;

        if (window.showSaveFilePicker)
        {
            try
            {
                handle = await window.showSaveFilePicker({ suggestedName: title });
                name = handle.name;
            }

            catch (e)
            {
                if (e.name === 'AbortError')
                {
                    picked(1, null);
                    return;
                }
            }
        }

        if (!handle)
        {
            name = window.prompt('TabPad', title);
        }

        if (!name)
        {
            picked(1, null);
            return;
        }

        if (handle)
        {
            handles.set('/' + name, handle);
            keep('handles', '/' + name, handle);
        }

        else
        {
            handles.delete('/' + name);
            keep('handles', '/' + name, undefined);
        }

        picked(1, '/' + name);
    }

    const env = {
        sess_put_open: function (name)
        {
            putting = { name: text_at(name), parts: [], size: 0 };

            return 0;
        },

        sess_put_write: function (buf, n)
        {
            if (!putting)
            {
                return -1;
            }

            putting.parts.push(bytes().slice(buf, buf + n));
            putting.size += n;

            return 0;
        },

        sess_put_close: function ()
        {
            const whole = new Uint8Array(putting ? putting.size : 0);
            let at = 0;

            if (!putting)
            {
                return -1;
            }

            for (const part of putting.parts)
            {
                whole.set(part, at);
                at += part.length;
            }

            blobs.set(putting.name, whole);
            keep('session', putting.name, whole);
            putting = null;

            return 0;
        },

        sess_get_open: function (name, size_at)
        {
            getting = blobs.get(text_at(name)) || null;
            got_at = 0;

            if (!getting)
            {
                return -1;
            }

            view().setUint32(size_at, getting.length, true);

            return 0;
        },

        sess_get_read: function (buf, n)
        {
            if (!getting || got_at + n > getting.length)
            {
                return -1;
            }

            bytes().set(getting.subarray(got_at, got_at + n), buf);
            got_at += n;

            return 0;
        },

        sess_get_close: function ()
        {
            getting = null;
        },

        sess_del: function (name)
        {
            const key = text_at(name);

            if (!blobs.delete(key))
            {
                return -1;
            }

            keep('session', key, undefined);

            return 0;
        },

        sess_list: function (first, name, max)
        {
            if (first)
            {
                listing = Array.from(blobs.keys());
                listed = 0;
            }

            while (listed < listing.length)
            {
                const raw = encoder.encode(listing[listed++]);

                if (raw.length < max)
                {
                    bytes().set(raw, name);
                    bytes()[name + raw.length] = 0;

                    return 0;
                }
            }

            return -1;
        },

        ed_draw_rect: function (x, y, w, h, colour)
        {
            ctx.fillStyle = rgb(colour);
            ctx.fillRect(x, y, w, h);
        },

        ed_draw_clip: function (x, y, w, h)
        {
            if (clipped)
            {
                ctx.restore();
            }

            ctx.save();
            ctx.beginPath();
            ctx.rect(x, y, w, h);
            ctx.clip();
            clipped = true;

            if (look)
            {
                ctx.font = look.font;
            }
        },

        ed_draw_text: function (x, y, text, n, colour)
        {
            const units = units_at(text, n);
            let pen = x;
            let i = 0;

            ctx.fillStyle = rgb(colour);

            while (i < n)
            {
                const cp = next_cp(units, i);

                if (cp > 0x20)
                {
                    ctx.fillText(String.fromCodePoint(cp), pen, y + look.ascent);
                }

                pen += width_of(cp);
                i += cp > 0xFFFF ? 2 : 1;
            }
        },

        ed_text_width: function (text, n)
        {
            const units = units_at(text, n);
            let pen = 0;
            let i = 0;

            while (i < n)
            {
                const cp = next_cp(units, i);

                pen += width_of(cp);
                i += cp > 0xFFFF ? 2 : 1;
            }

            return pen;
        },

        ed_text_font: function (which, percent)
        {
            const px = Math.round(BASE_PX * (window.devicePixelRatio || 1) * percent) / 100;
            const font = px + 'px ' + family(which ? 1 : 0);

            look = looks.get(font);
            ctx.font = font;

            if (!look)
            {
                const m = ctx.measureText('Mg');
                const up = m.fontBoundingBoxAscent || m.actualBoundingBoxAscent || px;
                const down = m.fontBoundingBoxDescent || m.actualBoundingBoxDescent || px / 4;

                look = { font: font, ascent: Math.round(up), height: Math.max(1, Math.ceil(up + down)), widths: new Map() };
                looks.set(font, look);
            }
        },

        ed_line_height: function ()
        {
            return look.height;
        },

        ed_font_count: function ()
        {
            return find_fonts().length;
        },

        ed_font_name: function (i)
        {
            find_fonts();

            return font_names[i] || scratch;
        },

        ed_font_use: function (which, name)
        {
            const wanted = text_at(name);

            if (which > 1 || (wanted && !find_fonts().includes(wanted)))
            {
                return -1;
            }

            chosen[which] = wanted;

            return 0;
        },

        ed_file_stat: function (path, size_at, mtime_at)
        {
            const file = files.get(text_at(path));

            if (!file)
            {
                return -1;
            }

            view().setBigUint64(size_at, BigInt(file.bytes.length), true);
            view().setBigUint64(mtime_at, BigInt(file.mtime), true);

            return 0;
        },

        ed_file_get: function (path, buf, n)
        {
            const file = files.get(text_at(path));

            if (!file || file.bytes.length < n)
            {
                return -1;
            }

            bytes().set(file.bytes.subarray(0, n), buf);

            return 0;
        },

        ed_file_put: function (path, buf, n)
        {
            const key = text_at(path);
            const raw = bytes().slice(buf, buf + n);
            const file = { bytes: raw, mtime: Date.now() };
            const handle = handles.get(key);

            files.set(key, file);
            keep('files', key, file);

            if (handle)
            {
                write_through(handle, raw).catch(function ()
                {
                    download(leaf(key), raw);
                });
            }

            else
            {
                download(leaf(key), raw);
            }

            return 0;
        },

        ed_clip_put: function (text, n)
        {
            clip = decoder.decode(bytes().subarray(text, text + n));

            if (!in_clip_event && navigator.clipboard && navigator.clipboard.writeText)
            {
                navigator.clipboard.writeText(clip).catch(function ()
                {
                });
            }

            return 0;
        },

        ed_clip_get: function (n_at)
        {
            const raw = encoder.encode(clip);

            api.web_free(clip_at);
            clip_at = raw.length ? api.web_alloc(raw.length + 1) : 0;

            if (!clip_at)
            {
                view().setUint32(n_at, 0, true);

                return 0;
            }

            bytes().set(raw, clip_at);
            view().setUint32(n_at, raw.length, true);

            return clip_at;
        },

        ed_pick: function (what)
        {
            if (what === 1)
            {
                pick_save();
            }

            else
            {
                pick_open();
            }
        },

        ed_title: function (name, unsaved)
        {
            title = text_at(name);
            document.title = title + (unsaved ? ' *' : '') + ' - TabPad';
        },

        ed_exit: function ()
        {
            api.ed_focus_lost();
            window.close();
        },

        web_confirm: function (text)
        {
            return window.confirm(text_at(text)) ? 1 : 0;
        },

        web_erase: function ()
        {
            const finish = function ()
            {
                window.location.reload();
            };

            stopped = true;
            unsettled.clear();
            forget_last_words();
            blobs.clear();
            files.clear();
            handles.clear();

            if (!db)
            {
                finish();
                return;
            }

            db.close();
            db = null;

            const request = indexedDB.deleteDatabase(DB_NAME);

            request.onsuccess = finish;
            request.onerror = finish;
            request.onblocked = finish;
        },

        web_stop: function ()
        {
            stopped = true;
            fail('TabPad stopped: it ran out of memory. What was typed up to the last pause is kept; reload the page.');

            throw new Error('stopped');
        },
    };

    async function unpack()
    {
        const packed = Uint8Array.from(atob(WASM), function (c)
        {
            return c.charCodeAt(0);
        });
        const stream = new Blob([packed]).stream().pipeThrough(new DecompressionStream('gzip'));

        return new Response(stream).arrayBuffer();
    }

    function on_key(e)
    {
        const mods = mods_of(e);
        const special = KEYS[e.key];
        const letter = e.key.length === 1 ? e.key.toLowerCase().charCodeAt(0) : 0;
        const held = mods & (CTRL | ALT);

        if (stopped || e.isComposing || (e.getModifierState && e.getModifierState('AltGraph')))
        {
            return;
        }

        if (special)
        {
            e.preventDefault();
            api.ed_key(special, mods);
            touched();
            return;
        }

        if (!letter || !held || (mods === CTRL && 'cxv'.includes(e.key.toLowerCase())))
        {
            return;
        }

        if (api.ed_key_cmd(letter, mods, scratch, scratch + 4) || (mods === ALT && MENU_LETTERS.includes(e.key.toLowerCase())))
        {
            e.preventDefault();
            api.ed_key(letter, mods);
            touched();
        }
    }

    function on_text()
    {
        const typed = input.value;

        input.value = '';

        if (typed && !stopped)
        {
            with_text(typed, function (at)
            {
                api.ed_text(at, encoder.encode(typed).length);

                return 0;
            });
            touched();
        }
    }

    function on_clip(e, letter)
    {
        if (stopped)
        {
            return;
        }

        e.preventDefault();
        in_clip_event = true;

        if (letter === 'v')
        {
            clip = e.clipboardData.getData('text/plain');
        }

        api.ed_key(letter.charCodeAt(0), CTRL);

        if (letter !== 'v')
        {
            e.clipboardData.setData('text/plain', clip);
        }

        in_clip_event = false;
        touched();
    }

    function on_wheel(e)
    {
        const unit = e.deltaMode === 1 ? WHEEL_STEP / 3 : e.deltaMode === 2 ? WHEEL_STEP * 10 : 1;
        let rows;

        e.preventDefault();
        wheel_rest += e.deltaY * unit;
        rows = Math.trunc(wheel_rest / WHEEL_STEP);

        if (rows && !stopped)
        {
            wheel_rest -= rows * WHEEL_STEP;
            api.ed_wheel(rows, mods_of(e));
            kick();
        }
    }

    async function on_drop(e)
    {
        const items = Array.from(e.dataTransfer.items || []).filter(function (item)
        {
            return item.kind === 'file';
        });

        e.preventDefault();

        for (const item of items)
        {
            const file = item.getAsFile();
            let handle = null;

            if (item.getAsFileSystemHandle)
            {
                handle = await item.getAsFileSystemHandle().catch(function ()
                {
                    return null;
                });
            }

            if (file && (!handle || handle.kind === 'file'))
            {
                picked(0, await take(file, handle));
            }
        }
    }

    function resized()
    {
        const [w, h, scale] = size();

        if (!stopped)
        {
            api.ed_resize(w, h, scale);
            kick();
        }
    }

    if (typeof WebAssembly !== 'object' || typeof DecompressionStream !== 'function')
    {
        fail('TabPad needs a browser from 2023 or later.');
        return;
    }

    db = await open_db();

    if (db)
    {
        await read_store('session', blobs);
        await read_store('files', files);
        await read_store('handles', handles);
        take_last_words();
    }

    try
    {
        const made = await WebAssembly.instantiate(await unpack(), { env: env });

        api = made.instance.exports;
        memory = api.memory;
    }

    catch (e)
    {
        fail('TabPad could not start in this browser.');
        return;
    }

    scratch = api.web_alloc(16);
    ctx.textBaseline = 'alphabetic';

    {
        const [w, h, scale] = size();

        if (api.ed_init(w, h, scale))
        {
            fail('TabPad could not open what it kept here. Erasing this site\'s data in the browser settings lets it start again.');
            return;
        }
    }

    input.addEventListener('keydown', on_key);
    input.addEventListener('input', function (e)
    {
        if (!e.isComposing)
        {
            on_text();
        }
    });
    input.addEventListener('compositionend', on_text);
    input.addEventListener('copy', function (e)
    {
        on_clip(e, 'c');
    });
    input.addEventListener('cut', function (e)
    {
        on_clip(e, 'x');
    });
    input.addEventListener('paste', function (e)
    {
        on_clip(e, 'v');
    });
    canvas.addEventListener('mousedown', function (e)
    {
        const [x, y] = point(e);

        e.preventDefault();
        input.focus({ preventScroll: true });

        if (e.button === 0 && !stopped)
        {
            api.ed_mouse_down(x, y, mods_of(e), e.detail || 1);
            kick();
        }
    });
    window.addEventListener('mousemove', function (e)
    {
        const [x, y] = point(e);

        if (!stopped)
        {
            api.ed_mouse_move(x, y);
            kick();
        }
    });
    window.addEventListener('mouseup', function (e)
    {
        if (e.button === 0 && !stopped)
        {
            api.ed_mouse_up();
            kick();
        }
    });
    canvas.addEventListener('wheel', on_wheel, { passive: false });
    canvas.addEventListener('contextmenu', function (e)
    {
        e.preventDefault();
    });
    window.addEventListener('dragover', function (e)
    {
        e.preventDefault();
    });
    window.addEventListener('drop', on_drop);
    window.addEventListener('resize', resized);
    window.addEventListener('blur', leave);
    window.addEventListener('pagehide', going);
    window.addEventListener('beforeunload', going);
    window.addEventListener('focus', function ()
    {
        input.focus({ preventScroll: true });
    });
    document.addEventListener('visibilitychange', function ()
    {
        if (document.visibilityState === 'hidden')
        {
            leave();
        }
    });
    setInterval(function ()
    {
        if (!stopped)
        {
            api.ed_tick(BigInt(Math.floor(performance.now())));
            kick();
        }
    }, TICK_MS);
    input.focus({ preventScroll: true });
    kick();
}());
