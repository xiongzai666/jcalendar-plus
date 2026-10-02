import {mkdir,readFile,writeFile,copyFile,readdir} from 'node:fs/promises';
import {resolve,dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const remote=resolve(dirname(fileURLToPath(import.meta.url)),'..');
export async function buildDemo(output=join(remote,'.demo-dist')){
  output=resolve(output);await mkdir(join(output,'demo'),{recursive:true});
  const publicDir=join(remote,'public');
  for(const item of await readdir(publicDir,{withFileTypes:true})){
    if(item.isFile()&&/\.(?:html|css|js|svg)$/.test(item.name))await copyFile(join(publicDir,item.name),join(output,item.name));
  }
  for(const item of await readdir(join(remote,'demo')))
    if(/\.(?:js|css)$/.test(item))await copyFile(join(remote,'demo',item),join(output,'demo',item));
  const state=await readFile(join(remote,'cloud-functions/lib/state.js'),'utf8');
  const start=state.indexOf('const DAYS ='),end=state.indexOf('export async function loadState');
  if(start<0||end<start)throw Error('Cloud validation source boundaries changed');
  const validation='// Generated from cloud validation; demo uses the same rules.\nconst utf8Length=value=>new TextEncoder().encode(value).length;\n'+state.slice(start,end).replaceAll('Buffer.byteLength','utf8Length');
  await writeFile(join(output,'config-validation.js'),validation);
  const wording=source=>source.replaceAll('保存到云端','保存演示').replaceAll('云端','演示数据').replaceAll('设备下次联网时应用','仅保存在当前浏览器').replaceAll('设备下次联网同步','仅在当前浏览器生效').replaceAll('设备将在下次联网时应用','仅在当前浏览器生效').replaceAll('设备已同步最新配置','模拟设备 · 已应用演示配置').replaceAll('最近连接','模拟连接').replaceAll('主 Wi‑Fi','示例网络').replaceAll('设备离线时无法实时上报失败原因；恢复联网后会补报最近一次故障和连续失败次数。','下方记录和运行摘要为模拟数据，不代表真实设备状态。').replaceAll('以下为设备上次联网时的报告；读取时间是获取天气的时间，预计联网时间可能随手动唤醒或后续刷新调整。','运行摘要是模拟示例，演示站不会获取天气或连接设备。').replaceAll('每两小时联网，并在实际放学前约 10 分钟更新天气；相近刷新会合并。短按上键也会同步。手机保存无法立即唤醒休眠中的设备。','这是功能演示：改动只保存在当前浏览器。真实设备需自行部署服务，并在下次联网时获取配置。');
  let html=await readFile(join(output,'index.html'),'utf8');
  html=wording(html).replace('lang="zh-CN"','lang="zh-CN" class="demo-root"').replaceAll(/((?:src|href)=")\/(?!\/)/g,'$1./').replace('<title>J-Calendar · 远程配置</title>','<title>J-Calendar Plus · 交互演示</title>').replace('墨水屏远程配置','墨水屏配置 · 交互演示').replace('设备连接','演示状态');
  html=html.replace('<head>',`<head>\n  <meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'none'; object-src 'none'; base-uri 'self'; form-action 'none'">`);
  html=html.replace('</head>','  <link rel="stylesheet" href="./demo/demo.css">\n</head>');
  html=html.replace('</header>',`</header>\n      <aside class="demo-banner" aria-label="演示说明"><div><strong>交互演示 · 示例数据</strong><p id="demo-storage-note">保存仅在当前浏览器，不会连接真实设备。</p></div><div class="demo-actions"><a href="https://github.com/xiongzai666/jcalendar-plus" target="_blank" rel="noopener noreferrer">查看源码</a><button id="demo-reset" class="secondary-button" type="button">恢复示例</button></div></aside>`);
  await writeFile(join(output,'index.html'),html);
  let app=await readFile(join(output,'app.js'),'utf8');
  if(!app.includes("from './edit-session.js'"))throw Error('App transport import changed');
  app="import './demo/ui.js';\n"+wording(app.replace("from './edit-session.js'","from './demo/session.js'"));
  app=app.replaceAll('saved = result.state; cloudRevision = saved.revision; stateEpoch++;', 'saved = result.state; device = result.device ?? device; cloudRevision = saved.revision; stateEpoch++;');
  app=app.replace("if (editsPending()) { event.preventDefault(); event.returnValue = ''; }", "if (!document.documentElement.dataset.demoReset && editsPending()) { event.preventDefault(); event.returnValue = ''; }");
  await writeFile(join(output,'app.js'),app);
  await writeFile(join(output,'.nojekyll'),'');
  await writeFile(join(output,'package.json'),'{"type":"module"}\n');
  // Validate every generated module and local import before upload.
  for(const directory of [output,join(output,'demo')])for(const name of await readdir(directory)){
    if(!name.endsWith('.js'))continue;
    const path=join(directory,name),check=spawnSync(process.execPath,['--check',path],{encoding:'utf8'});
    if(check.status!==0)throw Error(check.stderr);
    const source=await readFile(path,'utf8');
    for(const match of source.matchAll(/(?:from\s*|import\s*)['"](\.\.?\/[^'"]+)['"]/g))await readFile(resolve(directory,match[1]));
  }
  return output;
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url))console.log('Demo build:',await buildDemo(process.argv[2]));
