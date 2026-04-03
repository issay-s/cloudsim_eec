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
machine class:
{
        Number of machines: 8
        CPU type: ARM
        Number of cores: 16
        Memory: 16384
        S-States: [100, 85, 85, 65, 35, 8, 0]
        P-States: [10, 7, 5, 3]
        C-States: [10, 2, 1, 0]
        MIPS: [900, 700, 500, 350]
        GPUs: no
}
task class:
{
        Start time: 60000
        End time : 1200000
        Inter arrival: 1800
        Expected runtime: 2800000
        Memory: 16
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 110101
}
task class:
{
        Start time: 150000
        End time : 900000
        Inter arrival: 2400
        Expected runtime: 3500000
        Memory: 32
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA1
        CPU type: X86
        Task type: AI
        Seed: 110102
}
