import {demo,requestJSON} from './api.js';
const note=document.querySelector('#demo-storage-note');
note.textContent=demo.persistent?'保存仅在当前浏览器，不会连接真实设备。':'当前浏览器无法持久保存，修改在刷新后重置。';
document.querySelector('#demo-reset').addEventListener('click',async()=>{
  if(!confirm('恢复示例会清除本浏览器的演示改动和演示草稿，继续吗？'))return;
  await requestJSON('/api/demo/reset',{method:'POST'});
  document.documentElement.dataset.demoReset='1';
  location.reload();
});
