#!/usr/bin/env python3
"""Capture authored-map design previews using game_view.c and official RGB565 C.

This is a view QA facade, not a shipping integrated campaign or gameplay replay.
Python only converts RGB565, crops rendered tiles, stitches them, and makes a
contact sheet. Model tiles always come from the snapshot's real tower_tile API.
"""
import argparse
import contextlib
import ctypes as C
import hashlib
import importlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'assets_src/level_design'))
from tower_design_lab import build

FACADE = r'''#include "game.h"
#include "game_config.h"
#include "tower_floors.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"
#include <string.h>
static uint16_t pixels[GAME_WIDTH * GAME_HEIGHT];
static tower_handle_t model;
static game_snapshot_t snapshot;
static bool dirty;
esp_err_t mosaico_raylib_port_begin_frame(uint16_t **p,size_t *stride) {
 *p=pixels;*stride=GAME_WIDTH;return ESP_OK;
}
esp_err_t mosaico_raylib_port_present_frame(void) {return ESP_OK;}
const game_snapshot_t *game_read(game_handle_t game) {(void)game;return &snapshot;}
tower_tile_t game_tile(game_handle_t game,int x,int y) {
 (void)game;return tower_tile(model,snapshot.tower.floor,x,y);
}
bool game_needs_redraw(game_handle_t game) {(void)game;return dirty;}
void game_render_complete(game_handle_t game) {(void)game;dirty=false;}
int preview_init(void) {
 if(tower_create(NULL,&model)!=TOWER_OK) return -1;
 InitWindow(GAME_WIDTH,GAME_HEIGHT,"Authored campaign design preview");return 0;
}
void preview_close(void) {tower_delete(model);model=NULL;CloseWindow();}
int preview_frame(unsigned floor,unsigned cx,unsigned cy,unsigned hx,unsigned hy,
                  unsigned language,uint16_t *out) {
 if(!model||!out||floor>=TOWER_FLOORS) return -1;
 const tower_floor_desc_t *d=&tower_floor_table[floor];
 if(cx>(unsigned)(d->w>TOWER_VIEW_W?d->w-TOWER_VIEW_W:0) ||
    cy>(unsigned)(d->h>TOWER_VIEW_H?d->h-TOWER_VIEW_H:0)) return -2;
 memset(&snapshot,0,sizeof(snapshot));tower_read(model,&snapshot.tower);
 /* Camera-only view selection. Gameplay and the real model remain unchanged. */
 snapshot.tower.floor=floor;snapshot.tower.x=hx;snapshot.tower.y=hy;
 snapshot.screen=GAME_DUNGEON;snapshot.lang=(uint8_t)language;
 snapshot.volume=60;snapshot.brightness=GAME_BRIGHTNESS_DEFAULT;
 snapshot.camera_x=(uint16_t)cx;snapshot.camera_y=(uint16_t)cy;
 snapshot.floor_w=d->w;snapshot.floor_h=d->h;dirty=true;
 if(!game_render((game_handle_t)&snapshot)||dirty) return -3;
 memcpy(out,pixels,sizeof(pixels));return 0;
}
'''


def load_floors(path):
    if path.is_file():
        data = json.loads(path.read_text(encoding='utf-8'))
        if isinstance(data, dict):
            data = data.get('floors', data.get('maps'))
        if not isinstance(data, list):
            raise ValueError('Campaign JSON must contain a floor list')
        return data
    return [floor for region in range(1, 6)
            for floor in importlib.import_module(f'region{region}_blueprint').FLOORS]


def rows_of(floor):
    return list(floor['rows'] if isinstance(floor, dict) else floor.rows)


def engine_root(configured):
    candidates = [Path(configured)] if configured else [
        ROOT.parents[1] / 'submodule/raylib-lite-engine',
        Path('D:/espbulid/esp-mosaico-vibe/submodule/raylib-lite-engine')]
    for candidate in candidates:
        if (candidate / 'host/include/raylib.h').is_file():
            return candidate.resolve()
    raise RuntimeError('Official RGB565 engine unavailable; set MOSAICO_ENGINE_ROOT or --engine')


def compile_view(floors, out, engine):
    rules = importlib.import_module('campaign_rules')
    presentation = importlib.import_module('campaign_presentation')
    model_out = out / 'c-model'
    model_lib = build(floors, model_out, model_transform=rules.transform)
    model = model_out / 'model'
    originals = {}
    for name in ('game_view.c', 'game.h', 'game_config.h', 'ui_font.h'):
        blob = (ROOT / 'main' / name).read_bytes()
        originals[name] = hashlib.sha256(blob).hexdigest()
        (model / name).write_bytes(blob)
    view = presentation.transform_view((ROOT/'main/game_view.c').read_text(encoding='utf-8'))
    config = presentation.transform_config((ROOT/'main/game_config.h').read_text(encoding='utf-8'))
    (model/'game_view.c').write_text(view,encoding='utf-8')
    (model/'game_config.h').write_text(config,encoding='utf-8')
    # Full authored glyph subset is generated only in this lab snapshot.
    content = importlib.import_module('campaign_content')
    fontgen = importlib.import_module('gen_ui_font')
    corpus = view+json.dumps(content.NPCS,ensure_ascii=False)
    fontgen.OUT = model/'ui_font.h'
    fontgen.ui_characters = lambda: sorted({char for char in corpus if ord(char)>127})
    font_log = io.StringIO()
    with contextlib.redirect_stdout(font_log):
        if fontgen.main() != 0:
            raise RuntimeError('Temporary authored glyph generation failed')
    (out/'font-generation.log').write_text(font_log.getvalue(),encoding='utf-8')
    assert hashlib.sha256((ROOT/'main/ui_font.h').read_bytes()).hexdigest() == originals['ui_font.h'], 'production font changed during preview generation'
    (model / 'preview_facade.c').write_text(FACADE, encoding='utf-8')
    sources = [model/'preview_facade.c', model/'game_view.c', model/'tower.c', model/'floors.c',
               engine/'host/host_asset_runtime.c',
               *[engine/'components'/component/(component+'.c') for component in
                 ('mosaico_game_2d', 'mosaico_raylib_fast', 'mosaico_game_fx', 'mosaico_game_tilemap')]]
    includes = [model, engine/'host/include', engine/'host',
                *[engine/'components'/component/'include' for component in
                  ('mosaico_game_assets', 'mosaico_game_2d', 'mosaico_raylib_fast', 'mosaico_game_fx', 'mosaico_game_tilemap')]]
    cc = os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    env = os.environ.copy()
    env['PATH'] = str(Path(cc).parent) + os.pathsep + env.get('PATH', '')
    env['TMP'] = env['TEMP'] = str(out)
    library = out / ('campaign_view.dll' if os.name == 'nt' else 'campaign_view.so')
    command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-shared', '-DMOSAICO_HOST_SIMULATION=1']
    if os.name != 'nt':
        command += ['-fPIC']
    command += [str(path) for path in sources]
    for include in includes:
        command += ['-I', str(include)]
    command += ['-lm', '-o', str(library)]
    compiled = subprocess.run(command, capture_output=True, text=True, env=env)
    (out/'view-compile.log').write_text(compiled.stdout+compiled.stderr, encoding='utf-8')
    if compiled.returncode:
        raise RuntimeError('Actual C view build failed:\n'+compiled.stderr)
    lib = C.CDLL(str(library))
    lib._model_lib = model_lib
    lib.preview_init.argtypes = []
    lib.preview_close.argtypes = []
    lib.preview_close.restype = None
    lib.preview_frame.argtypes = [C.c_uint]*6 + [C.POINTER(C.c_uint16)]
    renderer_hashes = {str(path.relative_to(engine)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in sources if path.is_relative_to(engine)}
    (out/'view-provenance.json').write_text(json.dumps(dict(
        kind='DESIGN PREVIEW - camera QA facade, not shipping integrated campaign',
        engine=str(engine), official_rgb565_sources=renderer_hashes,
        game_view_sources=originals, facade_sha256=hashlib.sha256(FACADE.encode()).hexdigest(),
        presentation_transformed=True,
        presentation_transform_sha256=hashlib.sha256(Path(presentation.__file__).read_bytes()).hexdigest(),
        compiled_view_sha256=hashlib.sha256((model/'game_view.c').read_bytes()).hexdigest(),
        compiled_config_sha256=hashlib.sha256((model/'game_config.h').read_bytes()).hexdigest(),
        generated_font_sha256=hashlib.sha256((model/'ui_font.h').read_bytes()).hexdigest(),
        generated_floors_sha256=hashlib.sha256((model/'floors.c').read_bytes()).hexdigest(),
        authored_rows_sha256=hashlib.sha256(json.dumps([rows_of(floor) for floor in floors]).encode()).hexdigest(),
        command=command), indent=2), encoding='utf-8')
    return lib, model


def geometry(model):
    text = (model/'game_config.h').read_text(encoding='utf-8')
    names = ('GAME_WIDTH', 'GAME_HEIGHT', 'GAME_MAP_X', 'GAME_MAP_Y', 'GAME_TILE',
             'GAME_CAMERA_EDGE_HINT')
    values = {name: int(re.search(rf'\b{name}\s*=\s*(\d+)', text)[1]) for name in names}
    assert (values['GAME_WIDTH'], values['GAME_HEIGHT']) == (480, 480), values
    return values


def rgb565_image(buffer):
    # Pillow's packed little-endian RGB565 decoder converts the official
    # surface; no map interpretation or drawing occurs in Python.
    data = bytes(buffer)
    if sys.byteorder != 'little':
        data = b''.join(data[index:index+2][::-1] for index in range(0,len(data),2))
    return Image.frombytes('RGB',(480,480),data,'raw','BGR;16')


def intersects(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def subtract(rectangle, blocked):
    """Split a crop rectangle around an overlay, preserving literal pixels."""
    if not intersects(rectangle, blocked):
        return [rectangle]
    x0,y0,x1,y1 = rectangle
    bx0,by0,bx1,by1 = (max(x0,blocked[0]),max(y0,blocked[1]),
                       min(x1,blocked[2]),min(y1,blocked[3]))
    return [rect for rect in ((x0,y0,x1,by0),(x0,by1,x1,y1),
                              (x0,by0,bx0,by1),(bx1,by0,x1,by1))
            if rect[0] < rect[2] and rect[1] < rect[3]]


def camera_origins(size):
    maximum = max(size-11, 0)
    return sorted({0, maximum})


def capture(floors, out, engine, language):
    lib, model = compile_view(floors, out, engine)
    config = geometry(model)
    tile, left, top = config['GAME_TILE'], config['GAME_MAP_X'], config['GAME_MAP_Y']
    hint,extent = config['GAME_CAMERA_EDGE_HINT'],tile*11
    buffer = (C.c_uint16*(480*480))()
    assert lib.preview_init() == 0, 'actual C preview initialization failed'
    metadata, full_images = [], []
    try:
        for index, floor in enumerate(floors):
            rows = rows_of(floor)
            width, height = len(rows[0]), len(rows)
            positions = [(x,y) for y,row in enumerate(rows) for x,c in enumerate(row) if c == 'D']
            assert len(positions) == 1, ('expected one entry stair', index+1)
            entry = positions[0]
            alternate = next(((x,y) for y,row in enumerate(rows) for x,c in enumerate(row)
                              if c == 'U' and (x,y) != entry), None)
            if alternate is None:
                # Final floors can end at a goal rather than an upward stair.
                alternate = next(((x,y) for y,row in enumerate(rows) for x,c in enumerate(row)
                                  if c != '#' and (x,y) != entry), None)
            assert alternate is not None, ('expected another display position', index+1)

            def frame(cx, cy, hero):
                result = lib.preview_frame(index,cx,cy,*hero,language,buffer)
                assert result == 0, ('actual renderer failed', index+1,cx,cy,result)
                return rgb565_image(buffer)

            entry_camera = (min(max(entry[0]-5,0),max(width-11,0)),
                            min(max(entry[1]-5,0),max(height-11,0)))
            representative = frame(*entry_camera, entry)
            representative_path = out/f'floor{index+1:02d}-viewport.png'
            representative.save(representative_path)
            stitched = Image.new('RGB',(width*tile,height*tile))
            coverage = Image.new('L',stitched.size)
            cameras = []
            # Every pasted tile is a literal crop from a real C camera frame.
            # Select another player snapshot when the hero overlaps a crop.
            for cy in camera_origins(height):
                for cx in camera_origins(width):
                    cameras.append([cx,cy])
                    # Keep real translucent extension hints in viewport PNGs.
                    # Fullfloor crops recover those pixels from an overlapping
                    # C camera instead; nothing is redrawn or painted over.
                    overlays = []
                    if cx>0: overlays.append((0,0,hint,extent))
                    if cx<max(width-11,0): overlays.append((extent-hint,0,extent,extent))
                    if cy>0: overlays.append((0,0,extent,hint))
                    if cy<max(height-11,0): overlays.append((0,extent-hint,extent,extent))
                    for hero in (entry, alternate):
                        image = frame(cx,cy,hero)
                        safe = [(0,0,min(11,width-cx)*tile,min(11,height-cy)*tile)]
                        hx,hy = (hero[0]-cx)*tile,(hero[1]-cy)*tile
                        for overlay in overlays+[(hx,hy,hx+tile,hy+tile)]:
                            safe = [piece for rect in safe for piece in subtract(rect,overlay)]
                        for rect in safe:
                            crop = image.crop((left+rect[0],top+rect[1],left+rect[2],top+rect[3]))
                            destination = (cx*tile+rect[0],cy*tile+rect[1])
                            stitched.paste(crop,destination)
                            coverage.paste(Image.new('L',crop.size,255),destination)
            missing = coverage.histogram()[0]
            assert missing == 0, ('camera pixel coverage incomplete',index+1,missing)
            full_path = out/f'floor{index+1:02d}-fullfloor.png'
            stitched.save(full_path)
            full_images.append(stitched)
            metadata.append(dict(floor=index+1,width=width,height=height,entry=list(entry),
                                 entry_camera=list(entry_camera),cameras=cameras,
                                 tiles_cropped=width*height,pixels_cropped=width*height*tile*tile,
                                 viewport=representative_path.name,
                                 fullfloor=full_path.name))
            print(f'Captured design floor {index+1:02d}: {width}x{height}, {len(cameras)} C camera origins',flush=True)
    finally:
        lib.preview_close()
    # Labels and thumbnail assembly are outside the shipping 480x480 UI.
    cell_w, cell_h, columns = 240, 258, 10
    sheet = Image.new('RGB',(columns*cell_w,5*cell_h),(14,19,28))
    draw = ImageDraw.Draw(sheet)
    for index, image in enumerate(full_images):
        thumb = image.copy()
        thumb.thumbnail((232,232),Image.Resampling.NEAREST)
        x, y = (index%columns)*cell_w,(index//columns)*cell_h
        sheet.paste(thumb,(x+(cell_w-thumb.width)//2,y+22))
        draw.text((x+8,y+5),f'DESIGN {index+1:02d}  {metadata[index]["width"]}x{metadata[index]["height"]}',fill=(234,230,215))
    sheet.save(out/'campaign-contact-sheet.png')
    (out/'capture-manifest.json').write_text(json.dumps(dict(
        kind='DESIGN PREVIEW - actual C view with camera-only facade; not shipping integrated campaign',
        screen=[480,480],floors=metadata,contact_sheet='campaign-contact-sheet.png',
        stitch='Literal C-rendered map crops; overlapping cameras avoid 8px extension hints; alternate player snapshots avoid hero overlays.'),indent=2),encoding='utf-8')
    (out/'README.txt').write_text(
        'DESIGN PREVIEW / MAP QA, NOT SHIPPING INTEGRATED CAMPAIGN\n'
        'Actual copied main/game_view.c + official RGB565 engine. Tiles use tower_tile from\n'
        'the campaign_rules-transformed real C model snapshot. The facade selects floor,\n'
        'camera, and hero display coordinates for view QA without simulating gameplay.\n'
        'Viewport images retain the existing 480x480 UI. Fullfloor images stitch literal\n'
        'map crops from C camera frames; overlaps avoid extension-hint and hero overlays.\n'
        'The exact campaign_presentation transforms supply five regional palettes,\n'
        'fifty floor labels, dark mouths, and final hall.\n'
        'The renderer uses automatic edge paging without manual pan controls.\n'
        'Camera input/clamping and NPC dispatch/paging still require integration QA.\n',encoding='utf-8')
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps',type=Path,default=ROOT/'assets_src/level_design/campaign_maps.json')
    parser.add_argument('--out',type=Path,default=ROOT/'artifacts/campaign-design-preview')
    parser.add_argument('--engine',default=os.environ.get('MOSAICO_ENGINE_ROOT'))
    parser.add_argument('--language',choices=('zh','en'),default='zh')
    args = parser.parse_args()
    floors = load_floors(args.maps)
    assert len(floors) == 50, ('authored campaign must contain 50 floors',len(floors))
    out = args.out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    capture(floors,out,engine_root(args.engine),int(args.language == 'en'))
    print(f'PASS: 50 actual-C design viewports, 50 fullfloor crops, contact sheet: {out}')


if __name__ == '__main__':
    main()
