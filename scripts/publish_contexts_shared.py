#!/usr/bin/env python3
"""Publish an immutable qualified Contexts object, never overwrite a release."""
import argparse,hashlib,json,os,urllib.request,urllib.error,urllib.parse
from pathlib import Path
def digest(data):return hashlib.sha256(data).hexdigest()
def run(root):
 repo=os.environ['GITHUB_REPOSITORY'];commit=os.environ['GITHUB_SHA'];token=os.environ['GITHUB_TOKEN']
 if len(commit)!=40 or any(c not in '0123456789abcdef' for c in commit):raise ValueError('Exact source SHA required')
 tag='contexts-shared-0.5.0-dev-'+commit[:12]
 base='https://api.github.com/repos/'+repo
 def request(url,body=None,method=None,binary=False,raw_response=False):
  headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
  if raw_response:headers['Accept']='application/octet-stream'
  if body is not None:headers['Content-Type']='application/octet-stream' if binary else 'application/json'
  data=body if binary else None if body is None else json.dumps(body).encode()
  with urllib.request.urlopen(urllib.request.Request(url,data=data,headers=headers,method=method)) as response:
   raw=response.read();return raw if raw_response else json.loads(raw) if raw else None
 package=root/'shared-target';delivery=json.loads((package/'delivery.json').read_text())
 elf=package/'contexts/contexts.elf';manifest=package/'contexts/contexts.json'
 assert json.loads((package/'source-lock.json').read_text())['commits']['utilities']==commit
 assert digest(elf.read_bytes())==delivery['application']['sha256']
 assert len({x['sha256'] for x in delivery['deployments'].values()})==1
 evidence={}
 for lane in ('shared-faults','shared-faults-sanitized','shared-native','shared-native-sanitized','shared-paper'):
  data=(root/lane/'results.json').read_bytes();obj=json.loads(data)
  evidence[lane]={'sha256':digest(data),'result':obj}
 evidence_path=package/'qualification.json'
 evidence_path.write_text(json.dumps({'source_commit':commit,'lanes':evidence,'physical_testing':'not performed'},indent=2)+'\n')
 files={'contexts.elf':elf,'contexts.json':manifest,'build.json':package/'contexts/build.json',
        'delivery.json':package/'delivery.json','source-lock.json':package/'source-lock.json','qualification.json':evidence_path}
 try:release=request(base+'/releases/tags/'+urllib.parse.quote(tag,safe=''))
 except urllib.error.HTTPError as error:
  if error.code!=404:raise
  release=request(base+'/releases',{'tag_name':tag,'target_commitish':commit,
   'name':'Contexts0.5.0 shared ELF '+commit[:12],'draft':True,'prerelease':True,
   'body':'One byte-identical application ELF for X4, T-Watch and Waveshare. Provider-owned scenes/text/availability. Qualified owning host and target checks; matching provider/grant composition required. Physical testing not performed. Source '+commit})
 assets={a['name']:a for a in request(base+'/releases/'+str(release['id'])+'/assets')}
 bindings=[]
 for name,path in files.items():
  data=path.read_bytes();wanted=digest(data)
  if name in assets:
   asset=assets[name]
   if asset['size']!=len(data) or asset.get('digest')!='sha256:'+wanted:raise ValueError('Existing immutable asset differs: '+name)
  else:
   asset=request(release['upload_url'].split('{')[0]+'?name='+urllib.parse.quote(name),data,binary=True)
  if asset['size']!=len(data) or asset.get('digest')!='sha256:'+wanted:raise ValueError('Uploaded asset hash mismatch: '+name)
  downloaded=request(asset['url'],raw_response=True)
  if len(downloaded)!=len(data) or digest(downloaded)!=wanted:raise ValueError('Downloaded asset hash mismatch: '+name)
  bindings.append({'name':name,'size':len(data),'sha256':wanted,'url':asset['browser_download_url']})
 if release.get('draft'):
  release=request(base+'/releases/'+str(release['id']),{'draft':False,'prerelease':True},method='PATCH')
 # Draft asset URLs contain an untagged placeholder. Resolve final public
 # URLs only after publication and independently fetch them without a token.
 public_assets={a['name']:a for a in request(base+'/releases/'+str(release['id'])+'/assets')}
 for binding in bindings:
  asset=public_assets[binding['name']]
  assert asset['size']==binding['size'] and asset.get('digest')=='sha256:'+binding['sha256']
  with urllib.request.urlopen(asset['browser_download_url'],timeout=60) as response:public_bytes=response.read()
  if len(public_bytes)!=binding['size'] or digest(public_bytes)!=binding['sha256']:raise ValueError('Public download differs: '+binding['name'])
  binding['url']=asset['browser_download_url']
 receipt={'repo':repo,'tag':tag,'source_commit':commit,'release_url':release['html_url'],'assets':bindings,'physical_testing':'not performed'}
 (package/'publication.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True)
 run(p.parse_args().build)
