# recoll

데스크톱 원문 검색 프로그램

Desktop full-text search tool.

origin : https://www.lesbonscomptes.com/recoll/

![recoll](./imgs/recoll.png)

# TO-DO
 * 한글 인터페이스가 없으므로 번역을 깨끗하게 추가
 * hwp 원문 검색이 될 수 있도록 기능 개선
 * 검색결과 화면의 폰트가 너무 크게 나오고 있음. 검색결과 화면정리 필요

# How to apply patch

이 패키지는 원본 패키지를 유지한 채로 patch 를 적용하는 방식입니다.
아래 링크에서 데비안 패키지의 quilt 내용을 확인할 수 있습니다.
Ref : https://wiki.debian.org/UsingQuilt

업스트림의 내용을 수정하는 패키지 제작 방식에서는 원본 소스가 작업 폴더 상단에 있어야 합니다.

1) `recoll-1.26.3/` 폴더안의 소스코드를 수정 후 dpkg-source --commit 명령을 입력합니다.
2) 이 명령은 원본코드를 수정한 내용을 patch 로 만들어서 패키지에 적용하기 위한 과정을 자동으로 수행합니다
3) 패치의 이름과 내용을 입력하는 과정이 나오면 수정한 내용을 입력합니다.
4) dpkg-buildpackage 로 패키지를 제작합니다.