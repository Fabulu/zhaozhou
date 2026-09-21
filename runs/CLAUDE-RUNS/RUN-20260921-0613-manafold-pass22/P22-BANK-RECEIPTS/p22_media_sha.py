"""Probe + SHA-256 the 44 live pass-21 Manafold media files. Exit 1 on any error."""
import hashlib, json, os, subprocess, sys
site = r'C:\programmieren\zencrifice\manafold-p16\Upheaval\website'
root = r'C:\programmieren\zencrifice\manafold-p16\Upheaval\website\scratch-reel'
out = sys.argv[1]
subs = sorted(os.listdir(root)); assert len(subs) == 22
exp = {}
for s in subs:
    exp[s] = len([f for f in os.listdir(os.path.join(root, s)) if f.endswith('.rgb')])
lines, errs, total = [], [], 0
for s in subs:
    for ext in ('webm', 'png'):
        f = os.path.join(site, 'public', 'renders', f'{s}.{ext}')
        if not os.path.isfile(f) or os.path.getsize(f) == 0: errs.append(f'missing/empty {f}'); continue
        b = open(f, 'rb').read(); total += len(b)
        lines.append(f'{hashlib.sha256(b).hexdigest()}  {len(b):>9}  renders/{s}.{ext}')
        pr = json.loads(subprocess.run(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries',
              'stream=codec_name,width,height,pix_fmt,r_frame_rate,nb_read_frames','-of','json',f],
              capture_output=True,text=True,check=True).stdout)['streams'][0] if ext=='webm' else \
             json.loads(subprocess.run(['ffprobe','-v','error','-show_entries','stream=codec_name,width,height','-of','json',f],
              capture_output=True,text=True,check=True).stdout)['streams'][0]
        if ext == 'webm':
            ok = (pr['codec_name']=='vp9' and pr['width']==384 and pr['height']==240 and pr['pix_fmt']=='yuv444p'
                  and pr['r_frame_rate']=='60/1' and int(pr['nb_read_frames'])==exp[s])
        else:
            ok = pr['codec_name']=='png' and pr['width']==1152 and pr['height']==720
        if not ok: errs.append(f'{s}.{ext} probe {pr} expected frames {exp[s]}')
open(out,'w',newline='\n').write('# Manafold pass 22 live media: SHA-256, bytes, published path\n'
  f'# files={len(lines)} bytes={total} source=p22-scratch-reel-22 manifest=b188f2efa85649821bf5578bbfb1cea25511c214ea48040195d5a579b1777d6e\n'
  + '\n'.join(lines) + '\n')
print(f'files={len(lines)} bytes={total} errors={len(errs)}'); [print(' ',e) for e in errs]
sys.exit(1 if errs or len(lines)!=44 else 0)
