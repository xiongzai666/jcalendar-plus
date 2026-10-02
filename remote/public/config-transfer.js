export function exportConfig(state) {
  return {
    format: 'jcalendar-config',
    version: 1,
    exportedAt: new Date().toISOString(),
    revision: state.revision,
    settings: structuredClone(state.settings),
    todos: structuredClone(state.todos),
  };
}

export function importConfig(text) {
  if (text.length > 50000) throw new Error('备份文件过大');
  let data;
  try { data = JSON.parse(text); }
  catch { throw new Error('这不是有效的 JSON 备份文件'); }
  if (!data || data.format !== 'jcalendar-config' || data.version !== 1 ||
      !data.settings || typeof data.settings !== 'object' ||
      !Array.isArray(data.todos))
    throw new Error('备份格式不匹配，请选择 J-Calendar 导出的文件');
  return {
    settings: { ...data.settings, dateOverrides: data.settings.dateOverrides || [] },
    todos: data.todos,
  };
}
