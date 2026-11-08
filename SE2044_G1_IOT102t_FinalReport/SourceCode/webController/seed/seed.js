/*
TAO SEED 3 TAI KHOAN + ROLE TRONG FIREBASE (Auth + /users/role)

  user@nhom1.com   / 123456 -> role "user"
  admin@nhom1.com  / 123456 -> role "admin"
  esp32@nhom1.com  / 123456 -> role "device" (dung cho ESP32 ket noi RTDB)

  (Firebase bat mat khau toi thieu 6 ky tu -> khong dung duoc "123".)

CACH CHAY (mot lan):
  1. Firebase Console > Project settings > Service accounts
     > Generate new private key -> luu thanh serviceAccountKey.json
     DAT VAO CUNG THU MUC VOI FILE NAY.
  2. Sua databaseURL duoi day theo project.
  3. npm install firebase-admin
     node seed.js

  Script chay lai duoc: tai khoan da co se duoc cap nhat lai mat khau = 123456.

Neu khong muon chay script: tao/sua 3 tai khoan trong Firebase Console
(Authentication > Sign-in method > Email/Password > Add user),
sau do nhap role tay trong RTDB:  /users/<uid>/role = "user"|"admin"|"device".
*/
const admin = require('firebase-admin');

const serviceAccount = require('./serviceAccountKey.json');
const DATABASE_URL   = 'https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app';

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: DATABASE_URL,
});

const users = [
  { email: 'user@nhom1.com',  password: '123456', role: 'user' },
  { email: 'admin@nhom1.com', password: '123456', role: 'admin' },
  { email: 'esp32@nhom1.com', password: '123456', role: 'device' },
];

(async () => {
  for (const u of users) {
    let uid;
    try {
      uid = (await admin.auth().getUserByEmail(u.email)).uid;
      // tai khoan da ton tai -> dong bo lai mat khau moi
      await admin.auth().updateUser(uid, { password: u.password });
    } catch (e) {
      if (e.code !== 'auth/user-not-found') { console.error(u.email, e); continue; }
      const nu = await admin.auth().createUser({ email: u.email, password: u.password });
      uid = nu.uid;
    }
    await admin.database().ref('users/' + uid + '/role').set(u.role);
    console.log('OK:', u.email, '->', uid, 'role =', u.role, 'pass =', u.password);
  }
  process.exit(0);
})();
