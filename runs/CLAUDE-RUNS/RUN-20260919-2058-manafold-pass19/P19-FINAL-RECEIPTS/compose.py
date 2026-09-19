import sys
from PIL import Image
out=sys.argv[1]; ims=[Image.open(f'p19-final-sheets/P19-BANK-MANAFOLD-{s.upper()}-ALLFRAMES.png') for s in sys.argv[2:]]
w=max(i.width for i in ims); h=sum(i.height+6 for i in ims)
c=Image.new('RGB',(w,h),(255,255,255)); y=0
for i in ims: c.paste(i,(0,y)); y+=i.height+6
c.thumbnail((1600,1600)); c.save(out,quality=80); print(out,c.size)
