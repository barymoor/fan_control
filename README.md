# fan_control
Этот тул управляет вентиляторами antminer s9 с целью поддержания постоянной температуры чипов.
# Идея
В большинстве прошивок управление оборотами вентиляторов производится из модуля bmminer, который не поддерживает плавную регулировку и удержание температуры на постоянном уровне. Сам bmminer программирует вентиляторы с периодичностью от 10с до минуты, поэтому если отслеживать состояние регистра ШИМ чаще (100 мс в данном случае), то легко можно увидеть то, что записал туда bmminer, и поменять на нужное значение. Таким образом, контроль вентиляторов будет принадлежать уже не bmminer'у, а нашему тулу. Длительность пребывания "неправильного" значения в регистре не будет превышать 100мс, что, конечно, никак не отразится на скорости вращения вентиляторов.

Установка регистра ШИМ осуществляется методом [ПИД-регулирования](https://web.archive.org/web/20071007133550/http://logic-bratsk.ru/radio/pid/irt/main1_10.htm) Для считывания текущей температуры используется [cgminer api](https://github.com/ckolivas/cgminer/blob/master/API-README), а именно запрос estats, который поддерживается также и bmminer'ом. Если bmminer не запущен, утилита ожидает его появления, а если его нет более 2х минут, тормозит вентиляторы, чтобы не шумели (может быть полезно при отключении интернета)
# Как собирать
В основе лежит инструкция из [этого видео](https://www.youtube.com/watch?v=eD5F_KHkkqQ), возможно оно устарело, и есть более современные тулчейны, но я не вникал:

1. Потребуется ubuntu.
2. готовим систему
```
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install git build-essential libz1:i386 libc6:i386 libstdc++6:i386
```
3. Создаем папочку для работы в домашней папке, ставим тулчейн
```
mkdir projects
cd projects
mkdir build-tools
cd build-tools
wget https://releases.linaro.org/archive/12.11/components/toolchain/binaries/gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux.tar.bz2
tar xvf gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux.tar.bz2
git clone https://github.com/djp952/prebuilt-libz
cd ..
```
4. Нам потребуется библиотечка [jansson](https://github.com/akheron/jansson)
```
mkdir 3rdparty
cd 3rdparty
wget https://digip.org/jansson/releases/jansson-2.13.1.tar.gz
tar xvf jansson-2.13.1.tar.gz
cd jansson-2.13.1
XILINX_BASE_PATH="$HOME/projects/build-tools/gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux" PATH="$PATH:$XILINX_BASE_PATH/bin" ./configure --host=arm-linux-gnueabihf --prefix=$HOME/projects/build-tools/prebuilt-libz/linux-armhf --exec-prefix=$HOME/projects/build-tools/prebuilt-libz/linux-armhf
XILINX_BASE_PATH="$HOME/projects/build-tools/gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux" PATH="$PATH:$XILINX_BASE_PATH/bin" make
XILINX_BASE_PATH="$HOME/projects/build-tools/gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux" PATH="$PATH:$XILINX_BASE_PATH/bin" make install
cd ../..
```
5. Собираем fan_control
```
git clone https://github.com/barymoor/fan_control
cd fan_control
make
```

# Как запускать
1. При помощи scp копируем собранный fan_control куда-нибудь на s9. У меня он просто в /home/root
2. Заходим по ssh, делаем ```chmod +x fan_control```
3. Запускаем сислог ```syslogd -s 2000 -D```
4. Запускаем fan_control в фоне: ```fan_control 80&``` Здесь 80 - температура чипов для удержания. (Если ее удержать не удается, вентиляторы просто будут крутиться на максимальной скорости)
5. Любуемся на лог: ```tail -n 100 -f /var/log/messages```

При разрыве ssh сессии программа прибьется

# Как добавить в автозапуск
```
cd /etc/init.d
echo '#!/bin/sh' > syslogd.sh
echo 'syslogd -s 2000 -D' >> syslogd.sh
chmod +x syslogd.sh
echo '#!/bin/sh' > fan_control.sh
echo 'killall -9 fan_control || true' >> fan_control.sh
echo '/home/root/fan_control -d 80' >> fan_control.sh
chmod +x fan_control.sh
cd /etc/rcS.d
ln -s ../init.d/syslogd.sh S01syslogd.sh
ln -s ../init.d/fan_control.sh S80fan_control.sh
```

Надеюсь, не ошибся и ничего не упустил
