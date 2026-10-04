import fs from 'fs';

const wasm = fs.readFileSync(process.argv[2]);
const blobs = new Map();
let fails = 0;

function check(name, held)
{
    console.log((held ? 'ok    ' : 'FAIL  ') + name);
    fails += held ? 0 : 1;
}

async function run(type)
{
    const drawn = [];
    let api = null;
    let putting = null;
    let getting = null;
    let got_at = 0;
    let listing = [];
    let listed = 0;
    let title = '';

    function bytes()
    {
        return new Uint8Array(api.memory.buffer);
    }

    function view()
    {
        return new DataView(api.memory.buffer);
    }

    function text_at(at)
    {
        let end = at;

        while (bytes()[end])
        {
            end++;
        }

        return Buffer.from(bytes().subarray(at, end)).toString('utf8');
    }

    function nothing()
    {
    }

    function none()
    {
        return 0;
    }

    function refused()
    {
        return -1;
    }

    const env = {
        sess_put_open: function (name)
        {
            putting = { name: text_at(name), parts: [] };

            return 0;
        },

        sess_put_write: function (buf, n)
        {
            putting.parts.push(Buffer.from(bytes().slice(buf, buf + n)));

            return 0;
        },

        sess_put_close: function ()
        {
            blobs.set(putting.name, Buffer.concat(putting.parts));
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
            return blobs.delete(text_at(name)) ? 0 : -1;
        },

        sess_list: function (first, name, max)
        {
            if (first)
            {
                listing = Array.from(blobs.keys());
                listed = 0;
            }

            if (listed >= listing.length || listing[listed].length >= max)
            {
                return -1;
            }

            bytes().set(Buffer.from(listing[listed++] + '\0'), name);

            return 0;
        },

        ed_draw_text: function (x, y, text, n)
        {
            drawn.push(String.fromCharCode(...new Uint16Array(api.memory.buffer, text, n)));
        },

        ed_text_width: function (text, n)
        {
            return n * 8;
        },

        ed_line_height: function ()
        {
            return 16;
        },

        ed_font_use: function (which, name)
        {
            return text_at(name) ? -1 : 0;
        },

        ed_title: function (name)
        {
            title = text_at(name);
        },

        web_stop: function ()
        {
            throw new Error('out of memory');
        },

        ed_draw_rect: nothing,
        ed_draw_clip: nothing,
        ed_text_font: nothing,
        ed_pick: nothing,
        ed_exit: nothing,
        web_erase: nothing,
        ed_font_count: none,
        ed_font_name: none,
        ed_clip_put: none,
        ed_clip_get: none,
        web_confirm: none,
        ed_file_stat: refused,
        ed_file_get: refused,
        ed_file_put: refused,
    };

    api = (await WebAssembly.instantiate(wasm, { env })).instance.exports;

    if (api.ed_init(800, 600, 100))
    {
        return null;
    }

    if (type)
    {
        const raw = Buffer.from(type);
        const at = api.web_alloc(raw.length + 1);

        bytes().set(raw, at);
        api.ed_text(at, raw.length);
        api.web_free(at);
    }

    api.ed_draw();
    api.ed_focus_lost();

    return { drawn, title };
}

const first = await run('typed where no window is');

check('the module starts with nothing kept, and draws its menus', first && first.drawn.includes('File') && first.title === 'new 1');
check('what was typed is drawn', first && first.drawn.includes('typed where no window is'));
check('leaving writes the session: ' + Array.from(blobs.keys()).join(' '), blobs.size >= 2);

const second = await run('');

check('a second start finds the tab and its text', second && second.drawn.includes('typed where no window is') && second.title === 'new 1');
process.exit(fails ? 1 : 0);
