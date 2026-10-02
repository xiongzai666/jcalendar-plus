import {validateUpdate} from '../config-validation.js';
import {createDemoAPI} from './model.js';
let storage;
try{storage=localStorage;}catch{}
export const demo=createDemoAPI({storage,validateUpdate});
export const requestJSON=(path,options={})=>demo.request(path,options);
