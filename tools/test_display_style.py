"""Render current drawing bodies and real fonts; synthetic inputs, 400x300 pixels."""
import argparse, os, re, subprocess, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
environment = os.environ.get('J_CALENDAR_ENVIRONMENT', 'z15')
deps = root / '.pio/libdeps' / environment
lib = deps / 'U8g2_for_Adafruit_GFX/src'

def function(source, signature):
    start = source.index(signature)
    pos = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos] + '\n'

with tempfile.TemporaryDirectory(prefix='display-style-') as temp:
    build = Path(temp)
    view = (root / 'src/timetable_view.inc').read_text(encoding='utf8')
    signatures = ['static String fitText(', 'static String fitTextEllipsis(',
        'static void centered(', 'static void drawViewText(', 'static void drawWeatherCondition(',
        'static String lunarDayText(', 'static void drawFestivalCountdown(', 'static bool examCountdown(',
        'static const SubjectGlyph* subjectGlyph(', 'static int largeSubjectWidth(',
        'static void drawLargeSubject(', 'static void drawTodayLessons(',
        'static void drawTimetableCountdown(', 'static void drawDaytimeWeekly(',
        'static const char* syncFailureLabel(', 'static void drawTodoPage(']
    (build / 'timetable.inc').write_text(''.join(function(view, s) for s in signatures), encoding='utf8')
    screen = (root / 'src/screen_ink.cpp').read_text(encoding='utf8')
    layout = screen[screen.index('struct\n{'):screen.index('} static calLayout;')+len('} static calLayout;')]
    (build / 'calendar.inc').write_text(layout + '\n' + ''.join(function(screen, s) for s in
        ['void init_cal_layout_size()', 'void draw_cal_header()', 'void draw_cal_year(',
         'void draw_cal_days(', 'void draw_cd_day(', 'const char* getWeatherIcon(',
         'void draw_weather(']), encoding='utf8')
    radar = root / 'src/codex_radar_view.inc'
    (build / 'radar.inc').write_text(radar.read_text(encoding='utf8') if radar.exists() else '', encoding='utf8')
    font_source = (lib / 'u8g2_fonts.c').read_text(encoding='latin1')
    names = set(re.findall(r'u8g2_font_\w+', (build/'timetable.inc').read_text(encoding='utf8') +
        (build/'calendar.inc').read_text(encoding='utf8') + (build/'radar.inc').read_text(encoding='utf8')))
    names.update(['u8g2_font_wqy16_t_gb2312','u8g2_font_wqy12_t_gb2312'])
    definitions = []
    for n in sorted(names):
        source = (root/'src/u8g2_font_qweather_icon_16.c').read_text(encoding='utf8') if n == 'u8g2_font_qweather_icon_16' else font_source
        definition = re.search(r'const uint8_t '+n+r'\[.*?=\s*(?:"(?:\\.|[^"\\])*"\s*)+;',source,re.S).group()
        if n == 'u8g2_font_qweather_icon_16':definition = 'extern '+definition
        definitions.append(definition)
    (build/'fonts.cpp').write_text('#include "U8g2_for_Adafruit_GFX.h"\n'+'\n'.join(definitions),encoding='utf8')
    cxx = os.environ.get('J_CALENDAR_CXX','g++')
    compiler = [cxx,'c++'] if Path(cxx).stem == 'zig' else [cxx]
    binary = build/'style.exe'
    command = compiler + ['-std=c++11'] + ['-I'+str(v) for v in [root/'test/display_style_stub',
        root/'test',root/'include',deps/'ArduinoJson/src',deps/'Nongli/src',lib,build]]
    if radar.exists():command += ['-DHAS_RADAR']
    subprocess.run(command+[str(root/'test/display_style_host.cpp'),str(build/'fonts.cpp'),
        str(lib/'U8g2_for_Adafruit_GFX.cpp'),str(deps/'Nongli/src/nongli.cpp'),'-o',str(binary)],check=True)
    output = a.output.resolve() if a.output else build/'output'
    output.mkdir(parents=True,exist_ok=True)
    subprocess.run([str(binary),str(output)],check=True)
    if a.output:
        from PIL import Image
        for ppm in output.glob('*.ppm'):
            with Image.open(ppm) as im:im.save(ppm.with_suffix('.png'))
