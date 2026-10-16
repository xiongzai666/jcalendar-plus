export const orderedTodos = todos => [...todos].sort((a,b) => Number(a.done)-Number(b.done) ||
  (a.due || '9999').localeCompare(b.due || '9999') || Number(b.priority==='high')-Number(a.priority==='high'));
export class TodoEditSession {
  constructor() { this.value = null; }
  begin(todo) { this.value = {id:todo.id,title:todo.title,due:todo.due,priority:todo.priority}; }
  update(key,value) { if (this.value && ['title','due','priority'].includes(key)) this.value[key]=value; }
  cancel() { this.value = null; }
  apply(draft) {
    const edit=this.value, todo=draft.todos.find(item=>item.id===edit?.id);
    if (!todo) throw Error('待办已变动，请取消编辑后核对清单');
    const title=edit.title.trim();
    if (!title || [...title].length>16) throw Error('标题须为 1–16 字');
    if (edit.due && (!/^\d{4}-\d{2}-\d{2}$/.test(edit.due) || new Date(`${edit.due}T12:00:00Z`).toISOString().slice(0,10)!==edit.due)) throw Error('截止日期无效');
    if (!['normal','high'].includes(edit.priority)) throw Error('重要程度无效');
    Object.assign(todo,{title,due:edit.due,priority:edit.priority}); this.cancel(); return todo.id;
  }
}
export function syncResult({saved,revision,device,dirty=false,now=Date.now()}) {
  const reported=device?.reportedRevision ?? device?.appliedRevision;
  const at=Date.parse(device?.seenAt), stale=Number.isFinite(at)&&now-at>4*60*60*1000;
  const stamp=value=>Number.isFinite(Date.parse(value))?new Date(value).toLocaleString('zh-CN',{hour12:false}):'未知';
  let kind='waiting',headline='已保存，等待设备同步配置',action='短按设备上键同步配置，再重新检查。';
  if (dirty) {kind='draft';headline='修改还未保存到云端';action='完成编辑后，保存到云端。';}
  else if (revision===0 && !saved?.updatedAt) {kind='template';headline='云端模板待填写';action='替换模板并保存自己的配置，再核对设备。';}
  else if (stale) {kind='stale';headline='暂未收到新的设备报告';action='短按设备上键检查；离线原因尚不明确。';}
  else if (device && reported===revision) {kind='confirmed';headline=`设备已确认配置 v${revision}`;action='设备已确认收到配置，请核对实物屏幕。';}
  else if (Number.isFinite(at) && at>=Date.parse(saved?.updatedAt)) {kind='connected';headline='设备已连接，新配置尚未确认';action='查看最近同步记录，核对配置读取与确认结果。';}
  return {kind,headline,action,detail:`云端配置 v${revision ?? '未知'} · 设备回报 v${reported ?? '未知'}`,
    evidence:`云端保存 ${stamp(saved?.updatedAt)} · 最近设备报告 ${stamp(device?.seenAt)}`};
}
export function safeDiagnostic({revision,dirty=false,device}={}) {
  const integer=value=>Number.isInteger(value)&&value>=0?value:'未知';
  const time=value=>Number.isFinite(Date.parse(value))?new Date(value).toISOString():'未知';
  const runtime=device?.runtime;
  return {format:1,cloudRevision:integer(revision),deviceRevision:integer(device?.reportedRevision ?? device?.appliedRevision),
    reportTime:time(device?.seenAt),firmware:/^\d{1,3}\.\d{1,3}\.\d{1,3}(?:-(?:radar(?:-(?:minutely-)?validation)?|rc\.\d{1,3}))?(?![\s\S])/.test(runtime?.firmware)?runtime.firmware:'未知',driver:'未知',
    lastPage:[0,1,2,3].includes(runtime?.activePage)?runtime.activePage:'未知',runtimeReportTime:time(runtime?.reportedAt),
    lastError:['wifi','connect','auth','service','data','storage','config','time','ack'].includes(device?.lastFailure?.code)?device.lastFailure.code:'未知',
    hasLocalDraft:!!dirty,physicalDisplay:'未确认'};
}
