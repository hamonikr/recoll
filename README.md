# recoll

데스크톱 원문 검색 프로그램

Desktop full-text search tool.
 * ppt, doc, xls, pdf 등 원문검색 지원
 * hwp 문서 형식 원문검색 지원

origin : https://www.lesbonscomptes.com/recoll/

![recoll-3](./imgs/recoll-3.png)

![recoll-2](./imgs/recoll-2.png)

![recoll](./imgs/recoll.png)

# Install

## HamoniKR (>= 4.0)
```
sudo apt update
sudo apt install -y recoll=1.31.0-1hamonikr5
```

## Ubuntu (>= 20.04)
```
curl -sL https://pkg.hamonikr.org/add-hamonikr.apt | sudo -E bash -

sudo apt install -y recoll=1.31.0-1hamonikr6
```

## How to build from source for othrt linux

### Install build dependencies

```
sudo apt install -y bison debhelper dh-python dpkg-dev libaspell-dev libchm-dev libqt5webkit5-dev libx11-dev libxapian-dev libxslt1-dev libz-dev python3-all-dev python3-setuptools qtbase5-dev python3-pip qttools5-dev-tools docbook-xsl

```
### Build from source

```
sudo apt install -y python3-pip
sudo pip3 install pyhwp

./configure
make
sudo make install
```

## How to build debian package

우분투 24.04 에서는 시스템 전체에 pip 패키지를 설치해야 하는 경우, 
다음과 같이 PEP 668 정책을 우회하여 설치할 수 있다.  

* pip install 할 때 `--break-system-packages` 플래그를 사용하여 설치

하지만 좋은 방법은 다음과 같이 venv 를 이용해서 가상화 환경을 구성하고 그안에서 작업하는 것

```
python3 -m venv build-env
source build-env/bin/activate
pip install setuptools

dpkg-buildpackage -T clean
dpkg-buildpackage

dpkg -i ../recoll*.deb
```
