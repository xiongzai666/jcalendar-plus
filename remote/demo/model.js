export const DEMO_STATE_KEY = 'jcalendar.demo.state.v1';
export const DEMO_DRAFT_KEY = 'jcalendar.demo.draft.v1';
const clone = value => structuredClone(value);
const dayKey = date => new Date(date.getTime()+8*3600000).toISOString().slice(0,10);
export function createDemoSeed(date=new Date()) {
  const day=offset=>dayKey(new Date(date.getTime()+offset*86400000));
  const days=['一','二','三','四','五','六'];
  const subjects=['语文','数学','英语','物理','化学','地理','体育','自习'];
  const times=['08:00-08:45','08:55-09:40','09:50-10:35','10:45-11:30','14:00-14:45','14:55-15:40','15:50-16:35','16:45-17:30','18:30-19:15','19:25-20:10','20:20-21:05','21:15-22:00'];
  const labels=Array.from({length:8},(_,i)=>`第${i+1}节`).concat(Array.from({length:4},(_,i)=>`晚自习${i+1}`));
  return {schema:1,revision:1,updatedAt:date.toISOString(),settings:{
    countdownLabel:'示例考试',countdownDate:day(90).replaceAll('-',''),weatherLocation:'101010100',defaultPage:2,
    studySchedule:'444;'+days.map((day,index)=>day+','+Array.from({length:12},(_,slot)=>index===5&&slot>=6?'无课':subjects[(slot+index)%subjects.length]).join(',')+';').join(''),
    dailyRoutine:times.map((time,i)=>time+','+labels[i]+';').join(''),
    earlyReading:days.map((day,i)=>day+':'+(i%2?'英语':'语文')+';').join(''),
    dateOverrides:[{date:day(2),dayOff:true,lessons:[]},{date:day(3),dayOff:false,mode:'custom',lessons:[{slot:1,subject:'自习',start:'08:00',end:'09:30'},{slot:2,subject:'自习',start:'09:45',end:'11:30'},{slot:5,subject:'自习',start:'14:00',end:'15:30'}]}]},
    todos:[{id:'demo_reading',title:'阅读示例课文',due:day(0),priority:'high',done:false},{id:'demo_notes',title:'整理课程笔记',due:day(1),priority:'normal',done:false},{id:'demo_finished',title:'已完成示例',due:'',priority:'normal',done:true}]};
}
function fail(message,status){const error=new Error(message);error.status=status;throw error;}
export function createDemoAPI({storage,validateUpdate,now=()=>new Date()}) {
  let persistent=true,record;
  const fresh=()=>{const current=createDemoSeed(now());return {version:1,current,history:[clone(current)]};};
  const validState=state=>state && Number.isInteger(state.revision) && state.revision>=1 &&
    typeof state.updatedAt==='string' && Number.isFinite(Date.parse(state.updatedAt)) &&
    !validateUpdate({revision:state.revision,settings:state.settings,todos:state.todos});
  try{
    const text=storage?.getItem(DEMO_STATE_KEY);
    if(text&&text.length<=250000){
      const parsed=JSON.parse(text);
      if(parsed?.version===1&&validState(parsed.current)&&Array.isArray(parsed.history)&&parsed.history.length<=10&&parsed.history.length>0&&parsed.history.every(validState)&&
        parsed.history.every((item,index)=>item.revision===parsed.current.revision-index)&&JSON.stringify(parsed.history[0])===JSON.stringify(parsed.current))record=parsed;
    }
  }catch{ /* Corrupt or inaccessible demo storage falls back to the example. */ }
  record ||= fresh();
  const persist=()=>{try{if(!storage)throw Error('No storage');storage.setItem(DEMO_STATE_KEY,JSON.stringify(record));persistent=true;}catch{persistent=false;}};
  persist();
  const device=()=>({simulated:true,seenAt:now().toISOString(),reportedRevision:record.current.revision,appliedRevision:record.current.revision,
    lastNetwork:'primary',events:[{at:record.current.updatedAt,kind:'applied',revision:record.current.revision}],runtime:{firmware:'2.3.0-demo',reportedAt:now().toISOString(),nowReadAt:0,dailyReadAt:0,nowFailed:false,dailyFailed:false,nextNetworkAt:0,activePage:record.current.settings.defaultPage}});
  const save=current=>{record={version:1,current,history:[clone(current),...record.history].slice(0,10)};persist();return {state:clone(current),device:device(),demo:true,persistent};};
  const body=options=>{try{return JSON.parse(options.body||'{}');}catch{fail('演示数据不是有效 JSON',400);}};
  return {
    get persistent(){return persistent;},
    async request(path,options={}){
      const method=options.method||'GET';
      if(path==='/api/session'&&['GET','POST','DELETE'].includes(method))return method==='GET'?{authenticated:true}:{ok:true};
      if(path==='/api/admin/state'&&method==='GET')return {state:clone(record.current),device:device(),demo:true,persistent};
      if(path==='/api/admin/status'&&method==='GET')return {revision:record.current.revision,updatedAt:record.current.updatedAt,device:device(),demo:true,persistent};
      if(path==='/api/admin/history'&&method==='GET')return {versions:clone(record.history)};
      if(path==='/api/admin/validate'&&method==='POST'){const error=validateUpdate(body(options));if(error)fail(error,400);return {ok:true};}
      if(path==='/api/admin/state'&&method==='PUT'){
        const next=body(options),error=validateUpdate(next);if(error)fail(error,400);
        if(next.revision!==record.current.revision)fail('演示数据已有变化，请重新加载',409);
        return save({schema:1,revision:next.revision+1,updatedAt:now().toISOString(),settings:clone(next.settings),todos:clone(next.todos)});
      }
      if(path==='/api/admin/restore'&&method==='POST'){
        const input=body(options);if(input.revision!==record.current.revision)fail('演示数据已有变化，请重新加载',409);
        const old=record.history.find(item=>item.revision===input.restoreRevision&&item.revision<record.current.revision);
        if(!old)fail('这个演示历史版本不可恢复',400);
        return save({...clone(old),revision:record.current.revision+1,updatedAt:now().toISOString(),restoredFrom:old.revision});
      }
      if(path==='/api/demo/reset'&&method==='POST'){
        try{storage?.removeItem(DEMO_STATE_KEY);storage?.removeItem(DEMO_DRAFT_KEY);}catch{}
        record=fresh();persist();return {ok:true};
      }
      fail('演示站不提供这个接口',404);
    }
  };
}
