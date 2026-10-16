import { createHash, randomBytes, scryptSync } from 'node:crypto';
import { createInterface } from 'node:readline/promises';
import { stdin, stdout } from 'node:process';

const prompt = createInterface({ input: stdin, output: stdout });
const password = await prompt.question('设置手机网页管理密码（12–256 个字符）：');
prompt.close();
if (password.length < 12 || password.length > 256) {
  console.error('密码长度须为 12–256 个字符。');
  process.exitCode = 1;
} else {
  const salt = randomBytes(16);
  const deviceToken = randomBytes(32).toString('base64url');
  console.log('\n在 EdgeOne 项目环境变量中分别设置：');
  console.log(`ADMIN_PASSWORD_SCRYPT=${salt.toString('hex')}:${scryptSync(password, salt, 64).toString('hex')}`);
  console.log(`SESSION_SECRET=${randomBytes(32).toString('hex')}`);
  console.log(`DEVICE_TOKEN_SHA256=${createHash('sha256').update(deviceToken).digest('hex')}`);
  console.log('\n以下设备 Token 原文仅填写到设备本机配置页；不要用作网页管理密码，也不要提交到公开仓库：');
  console.log(`设备令牌=${deviceToken}`);
  console.log('\n请妥善保存这些值。关闭终端后，设备令牌不会再次显示。');
}
