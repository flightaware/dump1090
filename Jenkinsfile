node(label: 'raspberrypi') {
    def dists = ["stretch", "jessie"]
    def srcdir = "${WORKSPACE}/src"

    stage('Checkout') {
        sh "rm -fr ${srcdir}"
        sh "mkdir ${srcdir}"
        dir(srcdir) {
            checkout scm
        }
    }

    for (int i = 0; i < dists.size(); ++i) {
        def dist = dists[i]
        def pkgdir = "package-${dist}"
        def results = "results-${dist}"

        stage("Prepare source for ${dist}") {
            sh "rm -fr ${pkgdir}"
            sh "${srcdir}/prepare-build.sh ${dist} ${pkgdir}"
        }

        stage("Build for ${dist}") {
            sh "rm -fr ${results}"
            sh "mkdir -p ${results}"
            dir(pkgdir) {
                sh "DIST=${dist} pdebuild --use-pdebuild-internal --debbuildopts -b --buildresult ${WORKSPACE}/${results}"
            }
            archiveArtifacts artifacts: "${results}/*.deb", fingerprint: true
        }

        stage("Test install on ${dist}") {
            sh "/build/repo/validate-packages.sh ${dist} ${results}/dump1090-fa_*.deb ${results}/dump1090_*.deb"
        }
    }

    if (env.BRANCH_NAME == "master" || env.BRANCH_NAME == "dev") {
        stage("Deploy to staging repo") {
            for (int i = 0; i < dists.size(); ++i) {
                def dist = dists[i]
                def results = "results-${dist}"
                sh "/build/repo/deploy-packages.sh ${dist} ${results}/*.deb"
            }
        }
    }
}
