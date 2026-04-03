machine class:
{
        Number of machines: 10
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
machine class:
{
        Number of machines: 10
        CPU type: X86
        Number of cores: 16
        Memory: 12288
        S-States: [100, 85, 85, 65, 35, 8, 0]
        P-States: [10, 7, 5, 3]
        C-States: [10, 2, 1, 0]
        MIPS: [900, 700, 500, 350]
        GPUs: no
}
machine class:
{
        Number of machines: 6
        CPU type: X86
        Number of cores: 12
        Memory: 32768
        S-States: [140, 120, 120, 95, 52, 14, 0]
        P-States: [14, 10, 7, 5]
        C-States: [14, 4, 2, 0]
        MIPS: [1200, 920, 700, 500]
        GPUs: yes
}
task class:
{
        Start time: 70000
        End time : 220000
        Inter arrival: 60000
        Expected runtime: 600000
        Memory: 12
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA1
        CPU type: X86
        Task type: AI
        Seed: 330301
}
task class:
{
        Start time: 90000
        End time : 220000
        Inter arrival: 70000
        Expected runtime: 700000
        Memory: 10
        VM type: WIN
        GPU enabled: no
        SLA type: SLA2
        CPU type: X86
        Task type: WEB
        Seed: 330302
}
task class:
{
        Start time: 100000
        End time : 220000
        Inter arrival: 80000
        Expected runtime: 900000
        Memory: 20
        VM type: LINUX_RT
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: HPC
        Seed: 330303
}
