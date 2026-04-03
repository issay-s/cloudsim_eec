machine class:
{
        Number of machines: 20
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
task class:
{
        Start time: 60000
        End time : 300000
        Inter arrival: 4500
        Expected runtime: 1400000
        Memory: 10
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 551001
}
task class:
{
        Start time: 700000
        End time : 980000
        Inter arrival: 5000
        Expected runtime: 1300000
        Memory: 10
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: X86
        Task type: STREAM
        Seed: 551002
}
task class:
{
        Start time: 350000
        End time : 650000
        Inter arrival: 18000
        Expected runtime: 2200000
        Memory: 14
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA3
        CPU type: X86
        Task type: HPC
        Seed: 551003
}
