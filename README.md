# recoll

데스크톱 원문 검색 프로그램

Desktop full-text search tool.
 * ppt, doc, xls, pdf 등 원문검색 지원
 * hwp 문서 형식 원문검색 지원

origin : https://www.lesbonscomptes.com/recoll/

![recoll](./imgs/recoll.png)


# TO-DO
 * 한글 인터페이스가 없으므로 번역을 깨끗하게 추가
 * 검색결과 화면의 폰트가 너무 크게 나오고 있음. 검색결과 화면정리 필요

# How to build
```
dpkg-buildpackage -T clean
dpkg-buildpackage -k9FA298A1E42665B8
```

# Install 
```
dpkg -i ../recoll*.deb
```