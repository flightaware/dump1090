node(label: 'raspberrypi') {
    properties([
        pipelineTriggers([
            upstream(threshold: 'SUCCESS',
                     upstreamProjects: "bladeRF/${env.BRANCH_NAME}")
        ]),
        disableConcurrentBuilds(),
        durabilityHint(hint: 'PERFORMANCE_OPTIMIZED')
    ])

    def srcdir = "${WORKSPACE}/src"

    stage('Checkout') {
        sh "rm -fr ${srcdir}"
        sh "mkdir ${srcdir}"
        dir(srcdir) {
            checkout scm
        }
    }

    def dist_arch_list = [
      ["bookworm", "armhf"],
      ["bookworm", "arm64"],
      ["bullseye", "armhf"],
      ["bullseye", "arm64"],
      ["buster", "armhf"]
    ]

    def pkgdirs = [:]
    def resultdirs = [:]
    for (int i = 0; i < dist_arch_list.size(); ++i) {
        def dist_and_arch = dist_arch_list[i]
        def dist = dist_and_arch[0]
        def arch = dist_and_arch[1]

        String pkgdir
        if (pkgdirs.containsKey(dist)) {
            pkgdir = pkgdirs[dist]
        } else {
            pkgdir = "pkg-${dist}"
            stage("Prepare source for ${dist}") {
                sh "rm -fr ${pkgdir}"
                sh "${srcdir}/prepare-build.sh ${dist} ${pkgdir}"
            }
            pkgdirs[dist] = pkgdir
        }

        def resultdir = "results-${dist}-${arch}"
        resultdirs[dist_and_arch] = resultdir
        stage("Build for ${dist} (${arch})") {
            sh "rm -fr ${resultdir}"
            sh "mkdir -p ${resultdir}"
            dir(pkgdir) {
                sh "DIST=${dist} BRANCH=${env.BRANCH_NAME} ARCH=${arch} pdebuild --use-pdebuild-internal --debbuildopts -b --buildresult ${WORKSPACE}/${resultdir} -- --override-config"
            }
            archiveArtifacts artifacts: "${resultdir}/*.deb", fingerprint: true
        }

        stage("Test install on ${dist} (${arch})") {
            sh "BRANCH=${env.BRANCH_NAME} ARCH=${arch} /build/pi-builder/scripts/validate-packages.sh ${dist} ${resultdir}/dump1090-fa_*.deb"
        }
    }

    stage('Deploy to internal repository') {
        for (int i = 0; i < dist_arch_list.size(); ++i) {
            def dist_and_arch = dist_arch_list[i]
            def dist = dist_and_arch[0]
            def arch = dist_and_arch[1]
            def resultdir = resultdirs[dist_and_arch]
            sh "/build/pi-builder/scripts/deploy.sh -distribution ${dist} -architectures ${arch} -branch ${env.BRANCH_NAME} ${resultdir}/*.deb"
        }
    }
}
