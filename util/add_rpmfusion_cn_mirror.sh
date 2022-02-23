#!/bin/bash

add_rpmfusion_withmirror() {
    local centos_ver=$1

    rm -f rpmfusion-free-updates.repo
    rm -f rpmfusion-free-updates-testing.repo
    rm -f rpmfusion-nonfree-updates.repo
    rm -f rpmfusion-nonfree-updates-testing.repo

    yum -y install --nogpgcheck https://mirrors.tuna.tsinghua.edu.cn/rpmfusion/free/el/rpmfusion-free-release-${centos_ver}.noarch.rpm https://mirrors.tuna.tsinghua.edu.cn/rpmfusion/nonfree/el/rpmfusion-nonfree-release-${centos_ver}.noarch.rpm

    for repofile in rpmfusion-free-updates.repo rpmfusion-free-updates-testing.repo rpmfusion-nonfree-updates.repo rpmfusion-nonfree-updates-testing.repo
    do
        sed -i "s|http://download1.rpmfusion.org|https://mirrors.tuna.tsinghua.edu.cn/rpmfusion|" /etc/yum.repos.d/$repofile
        sed -i "s|^#baseurl=|baseurl=|" /etc/yum.repos.d/$repofile
        sed -i "s|^mirrorlist=|#mirrorlist=|" /etc/yum.repos.d/$repofile
    done
}

# is rpmfusion already exists?
exists=$(yum repolist | grep -E '^rpmfusion-' | wc -l )
if [[ $exists -gt 0 ]]; then
    echo "rpmfusion repo already exists, will overwrite!"

    # only support centos version from 6 to 8
    centos_ver=$(cat /etc/os-release | grep -E '^VERSION_ID' | awk -F'=' '{print $2}' | sed 's/"//g')
    if [[ $centos_ver -eq 6 ]] || [[ $centos_ver -eq 7 ]] || [[ $centos_ver -eq 8 ]]; then
        add_rpmfusion_withmirror $centos_ver
    else
        echo "unknown centos version: $centos_ver"
        exit 1
    fi
fi
