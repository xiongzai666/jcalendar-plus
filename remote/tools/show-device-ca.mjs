import tls from 'node:tls';
import { X509Certificate } from 'node:crypto';

const host = process.argv[2];
if (!host || !/^[a-z0-9.-]{4,100}$/i.test(host)) {
  console.error('用法：node tools/show-device-ca.mjs config.example.com');
  process.exit(1);
}

const socket = tls.connect({ host, port: 443, servername: host, rejectUnauthorized: true });
socket.setTimeout(10000, () => socket.destroy(new Error('连接超时')));
socket.once('secureConnect', () => {
  let peer = socket.getPeerCertificate(true);
  while (peer.issuerCertificate && peer.issuerCertificate !== peer) peer = peer.issuerCertificate;
  const last = new X509Certificate(peer.raw);
  let root = null;
  if (last.subject === last.issuer && last.verify(last.publicKey)) root = last;
  else {
    root = tls.rootCertificates.map(pem => new X509Certificate(pem))
      .find(candidate => candidate.subject === last.issuer && last.verify(candidate.publicKey));
  }
  if (!root) {
    console.error('未能找到链路对应的根证书；请不要关闭设备端证书校验。');
    process.exitCode = 2;
  } else {
    console.log(root.toString());
  }
  socket.end();
});
socket.once('error', error => {
  console.error(`证书检查失败：${error.message}`);
  process.exitCode = 2;
});
