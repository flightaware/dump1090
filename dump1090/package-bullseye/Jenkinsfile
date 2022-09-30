node(label: 'raspberrypi') {
    properties([
        pipelineTriggers([
            upstream(threshold: 'SUCCESS',
                     upstreamProjects: "bladeRF/${env.BRANCH_NAME}")
        ]),
        disableConcurrentBuilds(),
        durabilityHint(hint: 'PERFORMANCE_OPTIMIZED')
    ])

    def dists = ["bullseye", "buster", "stretch"]
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
                sh "DIST=${dist} BRANCH=${env.BRANCH_NAME} pdebuild --use-pdebuild-internal --debbuildopts -b --buildresult ${WORKSPACE}/${results} -- --override-config"
            }
            archiveArtifacts artifacts: "${results}/*.deb", fingerprint: true
        }

        stage("Test install on ${dist}") {
            sh "BRANCH=${env.BRANCH_NAME} /build/pi-builder/scripts/validate-packages.sh ${dist} ${results}/dump1090-fa_*.deb"
        }
    }

    stage('Deploy to internal repository') {
        for (int i = 0; i < dists.size(); ++i) {
            def dist = dists[i]
            def results = "results-${dist}"
            sh "/build/pi-builder/scripts/deploy.sh -distribution ${dist} -branch ${env.BRANCH_NAME} ${results}/*.deb"
        }
    }
}
