#!/usr/bin/env python3
"""Offline metadata inspection only; neither executes uploaded code nor emits firmware bytes."""
from pathlib import Path
import sys, json, hashlib, struct
REPO=Path('/workspace/jamboxfm')
UPSTREAM=Path('/workspace/scratch/fm1-device-plan/fm1-re')
sys.path.insert(0,str(REPO/'tools'))
from fm1pkg_make import crc16, enc, sfc, chipkey_decode
from lz4blk import decompress
def decompress_loader(container):
 out=b'';pos=64
 while pos<len(container):
  clen,dlen=struct.unpack_from('<II',container,pos)
  chunk=decompress(container[pos+8:pos+8+clen],out)
  assert len(chunk)==dlen
  out+=chunk;pos+=8+clen
 assert pos==len(container)
 return out
def digest(data):return hashlib.sha256(data).hexdigest()
def crypt(data):
 b=bytearray(data);enc(b,0,len(b));return bytes(b)
def entry(data):
 hcrc,dcrc,off,size,flags,reserved,index,name=struct.unpack('<HHIIBBH16s',data)
 assert hcrc==crc16(data[2:]),'bad JLFS header crc'
 return dict(name=name.split(b'\0')[0].decode('ascii'),offset=off,size=size,flags=hex(flags),reserved=reserved,index=index,header_crc=hex(hcrc),data_crc=hex(dcrc))
def table(region,offset=0,decode=False):
 rows=[]
 for i in range(24):
  e=entry(crypt(region[offset+i*32:offset+(i+1)*32]) if decode else region[offset+i*32:offset+(i+1)*32])
  rows.append(e)
  if e['index']:break
 return rows

def audit(path):
 raw=path.read_bytes();logical=b''.join(raw[i*48:i*48+47] for i in range(20))+raw[960:]
 product=''.join(chr((raw[i*48+47]-i-1)&255) for i in range(20) if raw[i*48+47]!=0x7d)
 h=crypt(logical[:64]);hcrc,lcrc,size,nent=struct.unpack_from('<HHIH',h)
 assert hcrc==crc16(h[2:]);assert lcrc==crc16(logical[64:64+nent*80]);assert size==len(logical)
 entries=[];contents={}
 for i in range(nent):
  e=crypt(logical[64+i*80:144+i*80]);typ,index,dcrc,unknown,off,dlen,aligned=struct.unpack_from('<HHHHIII',e)
  name=e[64:].split(b'\0')[0].decode('ascii');d=logical[off:off+dlen]
  assert len(d)==dlen
  if typ in (0,2,100,255):assert crc16(d)==dcrc,(name,hex(crc16(d)),hex(dcrc))
  entries.append(dict(name=name,type=typ,index=index,offset=off,length=dlen,aligned_length=aligned,crc16=hex(dcrc),stored_bytes_crc_matches=(crc16(d)==dcrc),sha256=digest(d)));contents[name]=d
 flash=contents['flash.bin'];head=flash[:0x4000]
 top=table(flash,32,True)
 for e in top:
  if e['name']=='isd_config.ini':
   blob=flash[e['offset']:e['offset']+e['size']];key=chipkey_decode(blob);assert crc16(blob[:32])==int.from_bytes(blob[32:34],'little')
  if e['name'] in ('uboot.boot','isd_config.ini'):
   data=flash[e['offset']:e['offset']+e['size']];e['stored_bytes_crc_matches']=(crc16(data)==int(e['data_crc'],0));e['sha256']=digest(data)
 app=bytearray(flash[0x4000:]);sfc(app,0,len(app),0,key)
 apphead=entry(app[:32]);appentries=table(app,32)
 assert crc16(app[32:apphead['size']])==int(apphead['data_crc'],0)
 for e in appentries:
  if e['name'] in ('app.bin','cfg_tool.bin'):
   data=app[e['offset']:e['offset']+e['size']];assert crc16(data)==int(e['data_crc'],0),(e,hex(crc16(data)));e['sha256']=digest(data)
 cfgstart=apphead['size'];cfghead=entry(app[cfgstart:cfgstart+32]);cfg=app[cfgstart:cfgstart+cfghead['size']]
 assert crc16(cfg[32:])==int(cfghead['data_crc'],0)
 cfge=entry(cfg[32:64]);assert crc16(cfg[cfge['offset']:cfge['offset']+cfge['size']])==int(cfge['data_crc'],0)
 ota=contents['ota.bin'];outer=entry(ota[:32]);assert crc16(ota[32:])==int(outer['data_crc'],0)
 inner=entry(ota[32:64]);expanded=decompress_loader(ota);assert len(expanded)==inner['offset'];assert crc16(expanded)==int(inner['data_crc'],0)
 blocks=[];pos=64
 while pos<len(ota):
  clen,dlen=struct.unpack_from('<II',ota,pos);blocks.append(dict(offset=pos,compressed_length=clen,decoded_length=dlen));pos+=8+clen
 assert pos==len(ota)
 out=dict(path=str(path),raw_size=len(raw),logical_size=len(logical),product=product,sha256=digest(raw),ufw_entries=entries,flash_head_sha256=digest(head),flash_head_top=top,chip_key=hex(key),app_area_header=apphead,app_entries=appentries,cfg_header=cfghead,cfg_eq=cfge,loader_outer=outer,loader_inner=inner,loader_decompressed_size=inner['offset'],loader_load_address=hex(inner['size']),loader_blocks=blocks,loader_decoded_sha256=digest(expanded),loader_structure_checked=True,jam_app_window_sha256=digest(flash[0x4000:0x93000]),checked_crc_scope='UFW header/list, flash.bin, ota.bin, info.log, tail.bin, nested loader, boot head entry headers and chip-key blob, app directory, app.bin, cfg_tool.bin, cfg/eq; opaque auxiliary UFW entries and encoded SPL payload not decoded')
 return out,contents
stock,sd=audit(Path('/workspace/deps/jambox-return/FM-1-V15.fwsc'))
jam,jd=audit(REPO/'build/felucca.fwsc')
sloop,ld=audit(Path('/workspace/deps/jambox-return/SLOOP-2.6.fwsc'))
out={'stock':stock,'jambox':jam,'sloop':sloop,'comparisons':{'stock_vs_jam_head_equal':sd['flash.bin'][:0x4000]==jd['flash.bin'][:0x4000],'stock_vs_sloop_head_equal':sd['flash.bin'][:0x4000]==ld['flash.bin'][:0x4000],'stock_loader_equals_pinned_reverse_engineering':sd['ota.bin']==(UPSTREAM/'analysis/device/ota-loader/ota_stock.bin').read_bytes(),'stock_head_preservation_required_for_current_official_return':True}}
print(json.dumps(out,indent=2))
