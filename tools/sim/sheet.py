import sys,glob,os
from PIL import Image,ImageDraw
files=sys.argv[3:]; out=sys.argv[1]; cols=int(sys.argv[2])
ims=[Image.open(f).convert('RGB') for f in files]
cw=max(i.width for i in ims)+10; ch=max(i.height for i in ims)+24
rows=(len(ims)+cols-1)//cols
S=Image.new('RGB',(cols*cw+10,rows*ch+10),(120,120,120)); d=ImageDraw.Draw(S)
for k,(f,i) in enumerate(zip(files,ims)):
    x=10+(k%cols)*cw; y=10+(k//cols)*ch
    d.text((x,y),os.path.basename(f)[:-4],fill=(255,255,255)); S.paste(i,(x,y+14))
S.save(out)
