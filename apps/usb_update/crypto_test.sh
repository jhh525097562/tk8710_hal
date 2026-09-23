#!/bin/bash

echo"===== 1. 准备阶段 ====="
# 创建一个测试文件
echo "RK3506 TurMass Gateway Update Content" > update.swu
echo "已创建测试文件: update.swu"

echo -e "\n===== 2. 生成密钥对 ====="
openssl genrsa -out private.pem 2048
openssl rsa -in private.pem -pubout -out public.pem
echo "私钥 (private.pem) 和 公钥 (public.pem) 已生成。"

echo -e "\n===== 3. 使用私钥生成签名 ====="
openssl dgst -sha256 -sign private.pem -out update.swu.sig update.swu
echo "签名文件 (update.swu.sig) 已生成。"

echo -e "\n===== 4. 使用公钥进行验证 ====="
openssl dgst -sha256 -verify public.pem -signature update.swu.sig update.swu

echo -e "\n===== 5. 模拟篡改测试 ====="
echo "Hacker modified this file!" >> update.swu
echo "文件内容已被修改，再次尝试验证："
openssl dgst -sha256 -verify public.pem -signature update.swu.sig update.swu
