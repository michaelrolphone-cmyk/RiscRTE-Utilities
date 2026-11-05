"""Native coordinate bounds and exclusive hit regions, without a framebuffer."""
import os
import importlib.util
from pathlib import Path
import subprocess
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "alarms_native_layout.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
static void bounds(alarms_native_rect r,int w,int h) {
 assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0);
 assert(r.x+r.w<=w&&r.y+r.h<=h);
 assert(alarms_native_inside(r,r.x,r.y));
 assert(alarms_native_inside(r,r.x+r.w-1,r.y+r.h-1));
 assert(!alarms_native_inside(r,r.x-1,r.y));
 assert(!alarms_native_inside(r,r.x,r.y-1));
 assert(!alarms_native_inside(r,r.x+r.w,r.y));
 assert(!alarms_native_inside(r,r.x,r.y+r.h));
}
static void group(alarms_native_rect *r,size_t n,int w,int h) {
 for(size_t i=0;i<n;i++) {
  bounds(r[i],w,h);
  for(size_t j=0;j<i;j++) assert(r[i].x+r[i].w<=r[j].x||
   r[j].x+r[j].w<=r[i].x||r[i].y+r[i].h<=r[j].y||r[j].y+r[j].h<=r[i].y);
 }
}
static void check(int w,int h,unsigned count) {
 alarms_native_layout l;assert(alarms_native_layout_make(&l,w,h,count));
 assert(l.width==w&&l.height==h);bounds(l.content,w,h);
 alarms_native_rect root[]={l.back,l.title,l.retry,l.primary,l.secondary,l.status,l.footer[0],l.footer[1]};
 group(root,sizeof(root)/sizeof(root[0]),w,h);
 alarms_native_rect fields[]={l.back,l.title,l.retry,l.fields[0],l.fields[1],l.done};
 group(fields,sizeof(fields)/sizeof(fields[0]),w,h);
 if(count==3) { bounds(l.fields[2],w,h);assert(l.fields[1].y+l.fields[1].h<=l.fields[2].y);assert(l.fields[2].y+l.fields[2].h<=l.done.y); }
 alarms_native_rect editor[]={l.back,l.title,l.retry,l.label,l.minus,l.value,l.plus,l.message,l.footer[0],l.footer[1]};
 group(editor,sizeof(editor)/sizeof(editor[0]),w,h);
}
int main(void) {
 alarms_native_layout l;
 assert(!alarms_native_layout_make(NULL,240,240,2));
 assert(!alarms_native_layout_make(&l,119,128,2));
 assert(!alarms_native_layout_make(&l,120,127,2));
 assert(!alarms_native_layout_make(&l,INT_MAX,128,2));
 assert(!alarms_native_layout_make(&l,120,INT_MAX,2));
 assert(!alarms_native_layout_make(&l,120,128,1));
 assert(!alarms_native_layout_make(&l,120,128,4));
 for(int w=120;w<=1024;w++)for(int h=128;h<=1024;h++)for(unsigned n=2;n<=3;n++)check(w,h,n);
 check(UINT16_MAX,UINT16_MAX,2);check(UINT16_MAX,UINT16_MAX,3);
 alarms_native_layout small,large;
 assert(alarms_native_layout_make(&small,240,240,2));
 assert(alarms_native_layout_make(&large,410,502,2));
 assert(small.back.w==large.back.w&&small.back.h==large.back.h);
 assert(small.footer[0].h==large.footer[0].h);
 return 0;
}
'''


class NativeAlarmsGeometry(unittest.TestCase):
    def test_capture_preserves_provider_pixels(self):
        spec = importlib.util.spec_from_file_location('alarms_capture', ROOT/'scripts/test_alarms_native.py')
        capture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(capture)
        with tempfile.TemporaryDirectory(prefix='alarms-capture-') as temporary:
            directory = Path(temporary)
            source, target = directory/'input.ppm', directory/'output.png'
            pixels = bytes(range(18))
            source.write_bytes(b'P6\n3 2\n255\n'+pixels)
            capture.png_from_ppm(source, target)
            data = target.read_bytes()
            self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack('>II', data[16:24]), (3, 2))
            offset, image = 8, b''
            while offset < len(data):
                length = struct.unpack('>I', data[offset:offset+4])[0]
                kind, body = data[offset+4:offset+8], data[offset+8:offset+8+length]
                self.assertEqual(zlib.crc32(kind+body), struct.unpack('>I', data[offset+8+length:offset+12+length])[0])
                if kind == b'IDAT':
                    image += body
                offset += length+12
            self.assertEqual(zlib.decompress(image), b'\0'+pixels[:9]+b'\0'+pixels[9:])
            source.write_bytes(b'P6\n3 2\n255\n'+pixels[:-1])
            with self.assertRaises(ValueError):
                capture.png_from_ppm(source, target)

    def test_exhaustive_supported_sizes(self):
        with tempfile.TemporaryDirectory(prefix='alarms-geometry-') as temporary:
            directory = Path(temporary)
            source, binary = directory/'geometry.c', directory/'geometry'
            source.write_text(PROBE)
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall',
                            '-Wextra', '-Werror', '-fsanitize=undefined',
                            '-fno-sanitize-recover=all', '-I'+str(ROOT/'Apps'),
                            str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == '__main__':
    unittest.main()
