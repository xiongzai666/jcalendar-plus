export * from '../edit-session.js';
import {DraftJournal as Original} from '../edit-session.js';
import {DEMO_DRAFT_KEY} from './model.js';
export class DraftJournal extends Original {
  constructor(storage){super(storage);this.key=DEMO_DRAFT_KEY;}
}
export {requestJSON} from './api.js';
