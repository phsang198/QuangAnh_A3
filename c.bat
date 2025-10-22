@echo off
setlocal enabledelayedexpansion

echo ========================================
echo     Git Quick Commit Push
echo ========================================
echo Dang o thu muc: %CD%
echo.

REM Kiem tra neu khong co commit message (tham so dau tien)
if "%*"=="" (
    echo Loi: Ban phai truyen commit message qua command line!
    echo Vi du: cmt.bat "default commit message"
    echo Hoac nhap message co dau cach trong dau ngoac kep.
    pause >nul
    exit /b 1
)

set "COMMIT_MSG=%*"

REM Buoc 1: Git add . (lan dau)
echo.
echo ========================================
echo GIT ADD
echo Dang them tat ca thay doi vao staging...
git add .
if %errorlevel% neq 0 (
    echo Loi khi chay git add. Nhan phim bat ky de thoat.
    pause >nul
    exit /b 1
)
echo [HOAN THANH]

REM Buoc 2: Commit voi message da truyen
echo.
echo ========================================
echo GIT COMMIT
echo Dang chay 'git commit -m "%COMMIT_MSG%"'...
git commit -m "%COMMIT_MSG%"
echo [HOAN THANH]

REM Buoc 3: Git push
echo.
echo ========================================
echo GIT PUSH
echo Dang day len remote (co the mat thoi gian neu file big size)...
git push
echo [HOAN THANH]

echo.
echo ========================================
echo     Hoan thanh toan bo quy trinh Git!
echo     Tong thoi gian: Tu dong (tuong tu chay tay).
echo ========================================
pause