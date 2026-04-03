machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 4096
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: no
}
task class:
{
        Start time: 60000
        End time : 350000
        Inter arrival: 18000
        Expected runtime: 900000
        Memory: 512
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 440401
}
task class:
{
        Start time: 90000
        End time : 350000
        Inter arrival: 24000
        Expected runtime: 1200000
        Memory: 640
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: X86
        Task type: STREAM
        Seed: 440402
}
